import os
import math
import json
import time
from datetime import datetime, timedelta

import requests
import numpy as np
import pandas as pd
from xgboost import XGBRegressor

# ==========================
# CONFIGURATION
# ==========================

API_KEY = "iEt9sFSDGEvS1qdxzKn9ggSLCyBBpG5Q"   # your Polygon key
TICKER  = "AAPL"                               # you can change this later

# How far back to train on (in days)
DAYS_BACK = 90

# Prediction horizons in minutes
HORIZONS = [1, 5, 15, 30, 60]

# Minute resolution for bars
MULTIPLIER = 1
TIMESPAN   = "minute"

# Where to save models
BASE_DIR   = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.join(BASE_DIR, "models")
os.makedirs(MODELS_DIR, exist_ok=True)


# ==========================
# HELPER FUNCTIONS
# ==========================

def polygon_agg_request(ticker, multiplier, timespan, from_date, to_date, api_key):
    """
    Call Polygon aggregates endpoint and return JSON.
    Docs: https://polygon.io/docs/stocks/get_v2_aggs_ticker__stocksTicker__range__multiplier____timespan___from___to
    """
    url = (
        f"https://api.polygon.io/v2/aggs/ticker/{ticker}/range/"
        f"{multiplier}/{timespan}/{from_date}/{to_date}"
        f"?adjusted=true&sort=asc&limit=50000&apiKey={api_key}"
    )
    resp = requests.get(url, timeout=30)
    resp.raise_for_status()
    return resp.json()


def download_agg_data(ticker, days_back=90):
    """
    Download minute bars for the last `days_back` days, handling day chunks.
    Returns a pandas DataFrame with columns:
      ['ts', 'open', 'high', 'low', 'close', 'volume', 'dt']
    """
    end_dt   = datetime.utcnow()
    start_dt = end_dt - timedelta(days=days_back)

    all_results = []

    cur_start = start_dt
    while cur_start < end_dt:
        cur_end = cur_start + timedelta(days=7)
        if cur_end > end_dt:
            cur_end = end_dt

        from_str = cur_start.strftime("%Y-%m-%d")
        to_str   = cur_end.strftime("%Y-%m-%d")
        print(f"Downloading {ticker} {from_str} -> {to_str} ...")

        data = polygon_agg_request(
            ticker=ticker,
            multiplier=MULTIPLIER,
            timespan=TIMESPAN,
            from_date=from_str,
            to_date=to_str,
            api_key=API_KEY
        )

        results = data.get("results", [])
        all_results.extend(results)

        cur_start = cur_end
        time.sleep(0.25)  # small delay to respect API

    if not all_results:
        raise RuntimeError("No data returned from Polygon. Check API key, plan, or ticker.")

    df = pd.DataFrame(all_results)

    # Polygon aggregate fields:
    # t: timestamp in ms
    # c: close, h: high, l: low, o: open, v: volume
    rename_map = {
        "t": "ts",
        "c": "close",
        "h": "high",
        "l": "low",
        "o": "open",
        "v": "volume",
    }
    df = df.rename(columns=rename_map)
    df = df[["ts", "open", "high", "low", "close", "volume"]].dropna()

    # Convert ts (ms) to datetime
    df["dt"] = pd.to_datetime(df["ts"], unit="ms", utc=True)
    df = df.sort_values("dt").reset_index(drop=True)

    return df


def add_features(df):
    """
    Add technical / ML features to the DataFrame.
    We will create:
      - returns
      - rolling volatility
      - SMA/EMA
      - RSI
      - volume-based features
      - lags
    """
    df = df.copy()

    # basic returns
    df["ret_1"] = df["close"].pct_change()
    df["ret_5"] = df["close"].pct_change(5)
    df["ret_15"] = df["close"].pct_change(15)

    # log returns
    df["log_ret_1"] = np.log(df["close"] / df["close"].shift(1))
    df["log_ret_5"] = np.log(df["close"] / df["close"].shift(5))
    df["log_ret_15"] = np.log(df["close"] / df["close"].shift(15))

    # moving averages
    for w in [5, 10, 20, 50]:
        df[f"sma_{w}"] = df["close"].rolling(w).mean()
        df[f"ema_{w}"] = df["close"].ewm(span=w, adjust=False).mean()

    # price relative to moving averages
    df["close_sma_20_diff"] = df["close"] / df["sma_20"] - 1.0
    df["close_ema_20_diff"] = df["close"] / df["ema_20"] - 1.0

    # rolling volatility (std dev of log returns)
    df["vol_20"] = df["log_ret_1"].rolling(20).std()
    df["vol_50"] = df["log_ret_1"].rolling(50).std()

    # RSI 14
    window = 14
    delta = df["close"].diff()
    up = np.where(delta > 0, delta, 0.0)
    down = np.where(delta < 0, -delta, 0.0)
    roll_up = pd.Series(up).rolling(window).mean()
    roll_down = pd.Series(down).rolling(window).mean()
    rs = roll_up / (roll_down + 1e-9)
    df["rsi_14"] = 100.0 - (100.0 / (1.0 + rs))

    # candle features
    df["hl_range"]   = (df["high"] - df["low"]) / df["close"]
    df["body_size"]  = (df["close"] - df["open"]) / df["open"]
    df["upper_wick"] = (df["high"] - df[["close", "open"]].max(axis=1)) / df["close"]
    df["lower_wick"] = (df[["close", "open"]].min(axis=1) - df["low"]) / df["close"]

    # volume features
    df["vol_chg"]    = df["volume"].pct_change()
    df["vol_sma_20"] = df["volume"].rolling(20).mean()
    df["vol_rel_20"] = df["volume"] / (df["vol_sma_20"] + 1e-9)

    # lags for some features
    lag_features = [
        "ret_1", "ret_5", "log_ret_1", "log_ret_5",
        "close_sma_20_diff", "close_ema_20_diff",
        "rsi_14", "vol_20", "hl_range", "body_size"
    ]
    for f in lag_features:
        for lag in [1, 2, 3, 5]:
            df[f"{f}_lag{lag}"] = df[f].shift(lag)

    df = df.dropna().reset_index(drop=True)
    return df


def make_horizon_target(df, horizon):
    """
    For a given horizon (in minutes), create a 'target' column:
      target = future close price at t + horizon
    """
    df = df.copy()
    df[f"target_{horizon}"] = df["close"].shift(-horizon)
    df = df.dropna().reset_index(drop=True)
    return df


def train_model_for_horizon(df, horizon, feature_cols):
    """
    Train XGBRegressor to predict close price horizon minutes ahead.
    """
    df_h = make_horizon_target(df, horizon)
    X = df_h[feature_cols].values
    y = df_h[f"target_{horizon}"].values

    # Simple train/validation split
    n = len(df_h)
    split = int(n * 0.8)
    X_train, X_val = X[:split], X[split:]
    y_train, y_val = y[:split], y[split:]

    model = XGBRegressor(
        n_estimators=300,
        max_depth=5,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        objective="reg:squarederror",
        tree_method="hist",
        random_state=42
    )

    print(f"\nTraining model for horizon = {horizon} minutes, "
          f"train size = {len(X_train)}, val size = {len(X_val)}")

    model.fit(X_train, y_train, eval_set=[(X_val, y_val)], verbose=False)

    # simple evaluation
    pred_val = model.predict(X_val)
    rmse = math.sqrt(np.mean((pred_val - y_val)**2))
    print(f"Validation RMSE for horizon {horizon}: {rmse:.4f}")

    return model


def main():
    print(f"Downloading data for {TICKER} over last {DAYS_BACK} days...")
    df = download_agg_data(TICKER, DAYS_BACK)
    print(f"Downloaded {len(df)} bars.")

    print("Adding features...")
    df_feat = add_features(df)
    print(f"Feature rows after dropna: {len(df_feat)}")

    # Choose features: all columns except time/targets/raw price columns
    drop_cols = ["ts", "dt", "open", "high", "low", "close", "volume"]
    feature_cols = [c for c in df_feat.columns if c not in drop_cols and not c.startswith("target_")]

    print(f"Number of features: {len(feature_cols)}")

    model_meta = {
        "ticker": TICKER,
        "horizons": {},
        "feature_cols": feature_cols,
    }

    for h in HORIZONS:
        model = train_model_for_horizon(df_feat, h, feature_cols)
        model_path = os.path.join(MODELS_DIR, f"xgb_{TICKER}_h{h}.json")
        print(f"Saving model for horizon {h} to: {model_path}")
        model.save_model(model_path)
        model_meta["horizons"][str(h)] = {
            "model_file": os.path.basename(model_path)
        }

    # Save metadata (feature list etc.)
    meta_path = os.path.join(MODELS_DIR, f"meta_{TICKER}.json")
    with open(meta_path, "w") as f:
        json.dump(model_meta, f, indent=2)
    print(f"Saved metadata to: {meta_path}")


if __name__ == "__main__":
    main()
