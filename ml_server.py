# ml_server.py
#
# Live XGBoost prediction server for your C++ app.
# Loads JSON models and exposes:
#     POST /predict_from_series
# Input:
#   { "ticker": "QQQ", "horizon": 60, "closes": [ ... ] }
# Output:
#   { "prediction": <future_price>, "log_return": <y_pred> }

import os
import json
import math
from typing import Dict, Any, List

import numpy as np
import pandas as pd
import xgboost as xgb
from flask import Flask, request, jsonify

# Where models are stored
MODELS_DIR = "models"

MODELS: Dict[str, Dict[int, xgb.Booster]] = {}
METAS: Dict[str, Dict[str, Any]] = {}

app = Flask(__name__)

# ---------------------------------------------------------
# Feature Engineering (MUST MATCH train_xgb_models.py)
# ---------------------------------------------------------

def compute_rsi(series: pd.Series, period=14):
    delta = series.diff()
    gain = delta.clip(lower=0).rolling(period).mean()
    loss = (-delta.clip(upper=0)).rolling(period).mean()
    rs = gain / loss
    return 100 - (100 / (1 + rs))

def add_features(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()

    df["ret1"] = np.log(df["close"] / df["close"].shift(1))
    df["ret5"] = np.log(df["close"] / df["close"].shift(5))
    df["ret15"] = np.log(df["close"] / df["close"].shift(15))

    df["sma20"] = df["close"].rolling(20).mean()
    df["ema20"] = df["close"].ewm(span=20, adjust=False).mean()

    df["rsi14"] = compute_rsi(df["close"], 14)

    df["close_sma20_diff"] = df["close"] - df["sma20"]
    df["close_over_sma20"] = df["close"] / df["sma20"]

    df["vol10"] = df["ret1"].rolling(10).std()
    df["vol20"] = df["ret1"].rolling(20).std()

    df["vol_zscore"] = (
        df["volume"] - df["volume"].rolling(50).mean()
    ) / df["volume"].rolling(50).std()

    # Synthetic placeholders (NO multifactor at inference)
    df["news_sent"] = 0
    df["pcr"] = 1.0
    df["call_oi"] = 0
    df["put_oi"] = 0
    df["opt_bullish"] = 0
    df["vix"] = 15.0
    df["hour"] = list(range(len(df)))[-len(df):]
    df["is_london"] = 0
    df["is_newyork"] = 1
    df["is_powerhour"] = 0

    return df

def build_feature_vector_from_closes(closes: List[float], feature_cols: List[str]):
    if len(closes) < 25:
        raise ValueError("Not enough closes for feature generation")

    df = pd.DataFrame({"close": closes, "volume": [1.0] * len(closes)})
    df = add_features(df)

    df = df.dropna(subset=feature_cols)
    if df.empty:
        raise ValueError("No valid rows after feature engineering")

    row = df.iloc[-1]
    x = row[feature_cols].values.astype(np.float32).reshape(1, -1)
    last_close = float(row["close"])
    return x, last_close

# ---------------------------------------------------------
# Load all models on startup
# ---------------------------------------------------------
def load_all_models():
    MODELS.clear()
    METAS.clear()
    if not os.path.isdir(MODELS_DIR):
        print("NO MODELS DIRECTORY FOUND")
        return

    for f in os.listdir(MODELS_DIR):
        if f.startswith("meta_") and f.endswith(".json"):
            t = f[5:-5].upper()
            meta_path = os.path.join(MODELS_DIR, f)
            with open(meta_path, "r") as m:
                meta = json.load(m)

            METAS[t] = meta
            MODELS[t] = {}

            for h in meta["horizons"]:
                model_file = os.path.join(MODELS_DIR, f"xgb_{t}_h{h}.json")
                if os.path.exists(model_file):
                    booster = xgb.Booster()
                    booster.load_model(model_file)
                    MODELS[t][h] = booster
                    print(f"[LOADED] {model_file}")
                else:
                    print(f"[MISSING] {model_file}")

load_all_models()

# ---------------------------------------------------------
# API
# ---------------------------------------------------------
@app.route("/predict_from_series", methods=["POST"])
def predict_from_series():
    try:
        data = request.get_json(force=True)
    except:
        return jsonify({"error": "Invalid JSON"}), 400

    ticker = data.get("ticker", "").upper()
    horizon = int(data.get("horizon", 0))
    closes = data.get("closes", [])

    if ticker not in MODELS:
        return jsonify({"error": f"Ticker {ticker} not trained"}), 400
    if horizon not in MODELS[ticker]:
        return jsonify({"error": f"Horizon {horizon} not trained"}), 400

    meta = METAS[ticker]
    feature_cols = meta["feature_cols"]

    try:
        x, last_close = build_feature_vector_from_closes(closes, feature_cols)
    except Exception as e:
        return jsonify({"prediction": -1.0, "error": str(e)})

    booster = MODELS[ticker][horizon]
    dmat = xgb.DMatrix(x)
    log_ret = float(booster.predict(dmat)[0])
    future_price = last_close * math.exp(log_ret)

    return jsonify({
        "ticker": ticker,
        "horizon": horizon,
        "prediction": future_price,
        "log_return": log_ret
    })

if __name__ == "__main__":
    print("[ML SERVER] Running on http://localhost:6000")
    app.run(host="0.0.0.0", port=6000)






