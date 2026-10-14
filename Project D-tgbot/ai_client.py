"""
Клиент для работы с нейросетями (YandexGPT и Ollama)
Преобразует текст пользователя в JSON-команды для ESP32
"""

import json
import requests
import logging
from typing import Optional

import config

logger = logging.getLogger(__name__)

# Системный промпт для нейросети
SYSTEM_PROMPT = """Ты — мозг настольного робота-дракона по имени Дракошка.
Твоя задача — преобразовать сообщение пользователя в строгий JSON-объект с командой для микроконтроллера.
А также, если пользователь просто хочет пообщаться, поддержи его как добрый друг.

Доступные команды:
1. "greet" — приветствие (звук "привет, я твой дракошка")
2. "bye" — прощание
3. "sing" — пение
4. "dance" — танец (5 сервоприводов двигаются)
5. "rainbow" — радуга на животе
6. "change_color" — параметр "color" (red/blue/green/yellow/purple/white/orange/cyan)
7. "set_brightness" — параметр "value" (0-255)
8. "set_volume" — параметр "value" (0-100)
9. "set_mic_sensitivity" — параметр "value" (0-100)
10. "blink" — моргнуть глазами
11. "add_deadline" — сохранить дедлайн. Параметры: "title" (текст), "date" (YYYY-MM-DD), "time" (HH:MM), "difficulty" (1-5, опционально). ИСПОЛЬЗУЙ когда пользователь просит записать/напомнить/запланировать.
12. "list_deadlines" — показать список всех дедлайнов пользователя. ИСПОЛЬЗУЙ когда просит "покажи дедлайны/задачи/расписание".
13. "unknown" — если команда не распознана.

Текущая дата и день недели указаны в системном сообщении. Переводи относительные даты ("сегодня", "завтра", "в среду") в YYYY-MM-DD.

Примеры:
"привет" → {"command": "greet", "params": {}, "reply_to_user": "Привет! Я твой дракошка! 🐉"}
"напомни завтра в 18:00 сдать курсач" → {"command": "add_deadline", "params": {"title": "сдать курсач", "date": "2026-10-16", "time": "18:00", "difficulty": 4}, "reply_to_user": "Записала! Напомню завтра в 18:00 📝"}
"покажи дедлайны" → {"command": "list_deadlines", "params": {}, "reply_to_user": "Вот твои задачи:"}

Отвечай ТОЛЬКО валидным JSON без markdown-оберток."""


def _clean_json_response(ai_text: str) -> str:
    """Очистка ответа от markdown-оберток"""
    ai_text = ai_text.strip()
    if ai_text.startswith("```json"):
        ai_text = ai_text[7:]
    if ai_text.startswith("```"):
        ai_text = ai_text[3:]
    if ai_text.endswith("```"):
        ai_text = ai_text[:-3]
    return ai_text.strip()


def _parse_ai_response(ai_text: str) -> dict:
    """Парсинг ответа нейросети в словарь"""
    try:
        clean_text = _clean_json_response(ai_text)
        return json.loads(clean_text)
    except json.JSONDecodeError as e:
        logger.error(f"Ошибка парсинга JSON: {e}\nОтвет был: {ai_text}")
        return {
            "command": "unknown",
            "params": {},
            "reply_to_user": "Мой драконий мозг немного запутался, повтори команду."
        }


def ask_yandex(user_text: str) -> dict:
    """Запрос к YandexGPT"""
    headers = {
        "Authorization": f"Api-Key {config.YANDEX_API_KEY}",
        "x-folder-id": config.YANDEX_FOLDER_ID,
        "Content-Type": "application/json"
    }

    payload = {
        "modelUri": config.YANDEX_MODEL_URI,
        "completionOptions": {
            "stream": False,
            "temperature": 0.1,
            "maxTokens": "300"
        },
        "messages": [
            {"role": "system", "text": SYSTEM_PROMPT},
            {"role": "user", "text": user_text}
        ]
    }

    try:
        response = requests.post(
            config.YANDEX_URL,
            headers=headers,
            json=payload,
            timeout=config.AI_TIMEOUT
        )
        response.raise_for_status()

        result = response.json()
        ai_text = result['result']['alternatives'][0]['message']['text']

        return _parse_ai_response(ai_text)

    except requests.exceptions.Timeout:
        logger.error("Таймаут запроса к YandexGPT")
        return {
            "command": "unknown",
            "params": {},
            "reply_to_user": "Я думаю слишком долго... Попробуй ещё раз!"
        }
    except requests.exceptions.RequestException as e:
        logger.error(f"Ошибка сети при запросе к YandexGPT: {e}")
        return {
            "command": "unknown",
            "params": {},
            "reply_to_user": "Ой, я сейчас не могу сообразить, попробуй позже!"
        }


def ask_ollama(user_text: str) -> dict:
    """Запрос к локальной Ollama"""
    url = f"{config.OLLAMA_BASE_URL}/chat/completions"

    headers = {
        "Content-Type": "application/json"
    }

    payload = {
        "model": config.OLLAMA_MODEL,
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": user_text}
        ],
        "temperature": 0.1,
        "stream": False
    }

    try:
        response = requests.post(
            url,
            headers=headers,
            json=payload,
            timeout=config.AI_TIMEOUT
        )
        response.raise_for_status()

        result = response.json()
        ai_text = result['choices'][0]['message']['content']

        return _parse_ai_response(ai_text)

    except requests.exceptions.Timeout:
        logger.error("Таймаут запроса к Ollama")
        return {
            "command": "unknown",
            "params": {},
            "reply_to_user": "Я думаю слишком долго... Попробуй ещё раз!"
        }
    except requests.exceptions.RequestException as e:
        logger.error(f"Ошибка сети при запросе к Ollama: {e}")
        return {
            "command": "unknown",
            "params": {},
            "reply_to_user": "Ой, я сейчас не могу сообразить, попробуй позже!"
        }


def get_drakosh_command(user_text: str) -> dict:
    """
    Главная функция: отправляет текст пользователя в нейросеть
    и получает JSON с командой для ESP32
    """
    if config.AI_PROVIDER == "yandex":
        return ask_yandex(user_text)
    elif config.AI_PROVIDER == "ollama":
        return ask_ollama(user_text)
    else:
        logger.error(f"Неизвестный провайдер AI: {config.AI_PROVIDER}")
        return {
            "command": "unknown",
            "params": {},
            "reply_to_user": "Ошибка конфигурации нейросети."
        }