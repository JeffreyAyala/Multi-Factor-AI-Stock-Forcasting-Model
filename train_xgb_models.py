# train_xgb_models.py
#
# Multi-Factor XGBoost training pipeline
# Compatible with ml_server.py (Jeffrey version)
# Includes:
#   Technical indicators
#   Time-of-day session features
#   News sentiment
#   Options sentiment (PCR, OI)
#   VIX volatility level
#
# Outputs:
#   models/xgb_{TICKER}_h{H}.json
#   models/meta_{TICKER}.json

import os
import sys
import math
import json
import datetime as dt

from typing import List

import requests
import numpy as np
import pandas as pd
import xgboost as xgb

# ---------------------------------------------------------
# Multi-Factor Synthetic Inputs
# (Already used in your ml_server.py — MUST MATCH EXACTLY)
# ---------------------------------------------------------

def get_news_sentiment(ticker: str) -> float:
    """Synthetic constant sentiment — stable for training."""
    return 0.0

def get_options_sentiment(ticker: str):
    """Synthetic options sentiment."""
    return (
        1.00,     # PCR
        1500.0,   # call OI
        1500.0,   # put OI
        0.0       # bullishness
    )

def get_vix_level() -> float:
    """Synthetic VIX level for training."""
    return 18.0


# ---------------------------------------------------------
# CONFIG
# ---------------------------------------------------------

POLYGON_API_KEY = "iEt9sFSDGEvS1qdxzKn9ggSLCyBBpG5Q"

DEFAULT_TICKERS = ["AAPL", "SPY", "QQQ"]

HORIZONS = [1, 5, 15, 30, 60]

DAYS_BACK = 30


# ---------------------------------------------------------
# Polygon download helpers
# ---------------------------------------------------------

def iso_date(days_ago: int) -> str:
    today = dt.date.today()
    target = today - dt.timedelta(days=days_ago)
    return target.isoformat()


def download_polygon_minute_bars(ticker: str, days_back: int = DAYS_BACK) -> pd.DataFrame:
    to_date = iso_date(0)
    from_date = iso_date(days_back)

    url = (
        f"https://api.polygon.io/v2/aggs/ticker/{ticker}/range/1/minute/"
        f"{from_date}/{to_date}"
        f"?adjusted=true&sort=asc&limit=50000&apiKey={POLYGON_API_KEY}"
    )

    print(f"[{ticker}] Downloading: {url}")

    r = requests.get(url, timeout=20)
    r.raise_for_status()
    data = r.json()

    if data.get("results") is None:
        raise RuntimeError(f"No results for {ticker}: {data}")

    df = pd.DataFrame(data["results"])
    df = df.rename(columns={"t": "timestamp", "c": "close", "v": "volume"})
    df["timestamp"] = pd.to_datetime(df["timestamp"], unit="ms")

    return df.sort_values("timestamp").reset_index(drop=True)


# ---------------------------------------------------------
# Feature engineering
# ---------------------------------------------------------

def compute_rsi(series: pd.Series, period: int = 14) -> pd.Series:
    delta = series.diff()
    gain = delta.clip(lower=0).rolling(period).mean()
    loss = (-delta.clip(upper=0)).rolling(period).mean()
    rs = gain / loss
    return 100 - (100 / (1 + rs))


def add_features(df: pd.DataFrame, ticker: str) -> pd.DataFrame:
    df = df.copy()

    # Technical features
    df["ret1"]  = np.log(df["close"] / df["close"].shift(1))
    df["ret5"]  = np.log(df["close"] / df["close"].shift(5))
    df["ret15"] = np.log(df["close"] / df["close"].shift(15))

    df["sma20"] = df["close"].rolling(20).mean()
    df["ema20"] = df["close"].ewm(span=20).mean()

    df["rsi14"] = compute_rsi(df["close"], 14)

    df["close_sma20_diff"] = df["close"] - df["sma20"]
    df["close_over_sma20"] = df["close"] / df["sma20"]

    df["vol10"] = df["ret1"].rolling(10).std()
    df["vol20"] = df["ret1"].rolling(20).std()

    df["vol_zscore"] = (df["volume"] - df["volume"].rolling(50).mean()) / (
        df["volume"].rolling(50).std()
    )

    # Multi-factor synthetic features (MUST MATCH ml_server.py)
    df["news_sent"] = get_news_sentiment(ticker)

    pcr, calloi, putoi, bullish = get_options_sentiment(ticker)
    df["pcr"] = pcr
    df["call_oi"] = calloi
    df["put_oi"] = putoi
    df["opt_bullish"] = bullish

    df["vix"] = get_vix_level()

    # Time-of-day features using timestamp hour
    df["hour"] = df["timestamp"].dt.hour
    df["is_london"]   = ((df["hour"] >= 3) & (df["hour"] <= 5)).astype(int)
    df["is_newyork"]  = ((df["hour"] >= 9) & (df["hour"] <= 11)).astype(int)
    df["is_powerhour"] = (df["hour"] >= 15).astype(int)

    return df


# ---------------------------------------------------------
# Supervised learning dataset
# ---------------------------------------------------------

def make_supervised_for_horizon(df: pd.DataFrame, horizon: int, feature_cols: List[str]):
    df = df.copy()
    df["target"] = np.log(df["close"].shift(-horizon) / df["close"])
    df = df.dropna(subset=feature_cols + ["target"])

    X = df[feature_cols].values.astype(np.float32)
    y = df["target"].values.astype(np.float32)

    return X, y, df


# ---------------------------------------------------------
# Train for each ticker
# ---------------------------------------------------------

def train_for_ticker(ticker: str):
    print(f"\n=== TRAINING {ticker} ===")

    df = download_polygon_minute_bars(ticker, DAYS_BACK)
    df = add_features(df, ticker)

    feature_cols = [
        "close", "volume",
        "ret1", "ret5", "ret15",
        "sma20", "ema20", "rsi14",
        "close_sma20_diff", "close_over_sma20",
        "vol10", "vol20", "vol_zscore",
        "news_sent",
        "pcr", "call_oi", "put_oi", "opt_bullish",
        "vix",
        "hour", "is_london", "is_newyork", "is_powerhour"
    ]

    df = df.dropna(subset=feature_cols).reset_index(drop=True)

    os.makedirs("models", exist_ok=True)

    meta = {
        "ticker": ticker,
        "feature_cols": feature_cols,
        "horizons": HORIZONS,
    }

    for h in HORIZONS:
        print(f"\n-- Horizon {h}m --")

        X, y, df_xy = make_supervised_for_horizon(df, h, feature_cols)

        if len(X) < 500:
            print(f"Skipping horizon {h}: only {len(X)} samples")
            continue

        n = len(X)
        split = int(0.8 * n)

        X_train, y_train = X[:split], y[:split]
        X_val, y_val = X[split:], y[split:]
        close_val = df_xy["close"].values[split:]

        model = xgb.XGBRegressor(
            n_estimators=400,
            max_depth=5,
            learning_rate=0.05,
            subsample=0.8,
            colsample_bytree=0.8,
            objective="reg:squarederror",
            n_jobs=4,
            random_state=42
        )

        model.fit(X_train, y_train)

        y_pred = model.predict(X_val)

        rmse = math.sqrt(float(np.mean((y_pred - y_val) ** 2)))
        print(f"[{ticker} h={h}] RMSE(return) = {rmse:.6f}")

        model_path = f"models/xgb_{ticker}_h{h}.json"
        model.get_booster().save_model(model_path)

    with open(f"models/meta_{ticker}.json", "w") as f:
        json.dump(meta, f, indent=2)

    print(f"[{ticker}] DONE.")


# ---------------------------------------------------------
# Main
# ---------------------------------------------------------

def main():
    tickers = sys.argv[1:] or DEFAULT_TICKERS
    for t in tickers:
        train_for_ticker(t.upper())


if __name__ == "__main__":
    main()

