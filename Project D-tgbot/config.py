
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

# === Ollama (локальная нейросеть, опционально) ===
OLLAMA_BASE_URL = os.getenv("OLLAMA_BASE_URL", "http://localhost:11434/v1")
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "qwen2.5:3b")

# === Выбор нейросети: "yandex" или "ollama" ===
AI_PROVIDER = os.getenv("AI_PROVIDER", "yandex")

# === ESP32 ===
ESP32_IP = os.getenv("ESP32_IP", "192.168.1.100")  # IP адрес дракошки в сети
ESP32_PORT = 80
ESP32_COMMAND_URL = f"http://{ESP32_IP}:{ESP32_PORT}/command"

# === Таймауты ===
AI_TIMEOUT = 10  # секунд на ответ от нейросети
ESP32_TIMEOUT = 5  # секунд на ответ от ESP32
MAX_RETRIES = 2  # количество повторных попыток отправки команды

# === Настройки по умолчанию ===
DEFAULT_BRIGHTNESS = 128  # яркость ленты 0-255
DEFAULT_VOLUME = 70  # громкость 0-100
DEFAULT_MIC_SENSITIVITY = 50  # чувствительность микрофона 0-100