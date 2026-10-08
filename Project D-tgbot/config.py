
"""
Конфигурация проекта Drakoshka
Хранит все настройки: токены, ключи API, параметры ESP32
"""
import os
from dotenv import load_dotenv

load_dotenv()

# === Telegram Bot ===
BOT_TOKEN = os.getenv("BOT_TOKEN", "")

# === YandexGPT ===
YANDEX_API_KEY = os.getenv("YANDEX_API_KEY", "")
YANDEX_FOLDER_ID = os.getenv("YANDEX_FOLDER_ID", "")
YANDEX_MODEL_URI = f"gpt://{YANDEX_FOLDER_ID}/yandexgpt/latest"
YANDEX_URL = "https://llm.api.cloud.yandex.net/foundationModels/v1/completion"

# === ESP32 через Cloudflare Relay (Вариант Б) ===
USE_RELAY = True
# Твой старый URL или новый, если создашь свежий
RELAY_URL = os.getenv("RELAY_URL", "https://helloesp32.ksushat75.workers.dev")

# === Прочее ===
AI_TIMEOUT = 10
MAX_RETRIES = 3  # Увеличили до 3, так как сетевые запросы через интернет могут иногда тупить

# === Настройки по умолчанию ===
DEFAULT_BRIGHTNESS = 128  # яркость ленты 0-255
DEFAULT_VOLUME = 70  # громкость 0-100
DEFAULT_MIC_SENSITIVITY = 50  # чувствительность микрофона 0-100