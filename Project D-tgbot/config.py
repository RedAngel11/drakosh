"""
Конфигурация проекта Drakoshka
"""
import os
from dotenv import load_dotenv

load_dotenv()

# === Telegram Bot ===
TOKEN = os.getenv("TOKEN", "")

# === ВЫБОР НЕЙРОСЕТИ: "yandex" или "ollama" ===
AI_PROVIDER = "yandex"   # <-- ВОТ ЭТА СТРОКА И БЫЛА ПРОПУЩЕНА

# === YandexGPT ===
YANDEX_API_KEY = os.getenv("YANDEX_API_KEY", "")
YANDEX_FOLDER_ID = os.getenv("YANDEX_FOLDER_ID", "")
YANDEX_MODEL_URI = f"gpt://{YANDEX_FOLDER_ID}/yandexgpt/latest"
YANDEX_URL = "https://llm.api.cloud.yandex.net/foundationModels/v1/completion"

# === Ollama (понадобится ТОЛЬКО если переключишь AI_PROVIDER на "ollama") ===
OLLAMA_BASE_URL = os.getenv("OLLAMA_BASE_URL", "http://localhost:11434/v1")
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "qwen2.5:3b")

# === ESP32 через Cloudflare Relay ===
USE_RELAY = True
RELAY_URL = os.getenv("RELAY_URL", "https://helloesp32.ksushat75.workers.dev")

# === Прочее ===
AI_TIMEOUT = 10
MAX_RETRIES = 3

# === Настройки по умолчанию ===
DEFAULT_BRIGHTNESS = 128
DEFAULT_VOLUME = 70
DEFAULT_MIC_SENSITIVITY = 50