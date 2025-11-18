from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn
import pandas as pd
import numpy as np
import json
import xgboost as xgb
import os

app = FastAPI()

MODEL_DIR = "./models"
POLYGON_KEY = "iEt9sFSDGEvS1qdxzKn9ggSLCyBBpG5Q"   # not used in series mode, but fine

# ---------- Load models & meta once ----------
with open(os.path.join(MODEL_DIR, "meta_AAPL.json"), "r") as f:
    META = json.load(f)

FEATURE_COLS = META["feature_cols"]

MODELS: dict[int, xgb.Booster] = {}
for h in [1, 5, 15, 30, 60]:
    booster = xgb.Booster()
    booster.load_model(os.path.join(MODEL_DIR, f"xgb_AAPL_h{h}.json"))
    MODELS[h] = booster

# ---------- Feature engineering ----------
def compute_features(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df["ret_1"] = df["close"].pct_change()
    df["ret_5"] = df["close"].pct_change(5)
    df["sma20"] = df["close"].rolling(20).mean()
    df["ema20"] = df["close"].ewm(span=20, adjust=False).mean()
    df["std20"] = df["close"].rolling(20).std()

    delta = df["close"].diff()
    gain = (delta.where(delta > 0, 0)).rolling(14).mean()
    loss = (-delta.where(delta < 0, 0)).rolling(14).mean()
    rs = gain / loss
    df["rsi14"] = 100 - (100 / (1 + rs))

    for lag in range(1, 6):
        df[f"lag_{lag}"] = df["close"].shift(lag)

    df.dropna(inplace=True)
    return df

# ---------- Body model for POST /predict_from_series ----------
class SeriesRequest(BaseModel):
    ticker: str
    horizon: int
    closes: list[float]

# ---------- New endpoint: no Polygon, just features + model ----------
@app.post("/predict_from_series")
def predict_from_series(req: SeriesRequest):
    """
    C++ sends: last N closes for a ticker.
    We compute features + XGBoost prediction and return a single number.
    """
    if len(req.closes) < 50:  # need enough history for features
        return {"error": "Not enough closes supplied"}

    df = pd.DataFrame({"close": req.closes})
    df_feat = compute_features(df)
    if df_feat.empty:
        return {"error": "Feature frame is empty after rolling windows"}

    last_row = df_feat.iloc[-1]
    x_input = last_row[FEATURE_COLS].values.astype(np.float32)
    dmatrix = xgb.DMatrix([x_input])

    booster = MODELS.get(req.horizon)
    if booster is None:
        return {"error": f"No model for horizon {req.horizon}"}

    pred = booster.predict(dmatrix)[0]
    return {"prediction": float(pred)}

# (optional) keep your old GET /predict here as backup if you want

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=6000)
