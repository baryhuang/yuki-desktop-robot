import os
import tempfile
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, File, Form, HTTPException, UploadFile
from faster_whisper import WhisperModel

MAX_UPLOAD_BYTES = 10 * 1024 * 1024
app = FastAPI(title="Yuki private STT")
whisper_model: Optional[WhisperModel] = None


@app.on_event("startup")
def load_model() -> None:
    global whisper_model
    whisper_model = WhisperModel(
        os.getenv("WHISPER_MODEL", "small"),
        device="cpu",
        compute_type=os.getenv("WHISPER_COMPUTE_TYPE", "int8"),
    )


@app.get("/health")
def health() -> dict[str, bool]:
    return {"ready": whisper_model is not None}


@app.post("/v1/audio/transcriptions")
async def transcribe(
    file: UploadFile = File(...),
    model: str = Form(...),
    language: Optional[str] = Form(None),
) -> dict[str, str]:
    del model
    if whisper_model is None:
        raise HTTPException(status_code=503, detail="Whisper model is still loading")

    audio = await file.read(MAX_UPLOAD_BYTES + 1)
    if len(audio) > MAX_UPLOAD_BYTES:
        raise HTTPException(status_code=413, detail="Audio upload is too large")
    if not audio:
        raise HTTPException(status_code=400, detail="Audio upload is empty")

    suffix = Path(file.filename or "audio.wav").suffix or ".wav"
    with tempfile.TemporaryDirectory() as directory:
        audio_path = Path(directory) / f"input{suffix}"
        audio_path.write_bytes(audio)
        segments, _ = whisper_model.transcribe(
            str(audio_path),
            language=language,
            beam_size=5,
            vad_filter=True,
        )
        text = "".join(segment.text for segment in segments).strip()

    return {"text": text}
