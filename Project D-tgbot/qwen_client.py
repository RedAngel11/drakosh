import json
from datetime import datetime

import aiohttp
from config import QWEN_API_KEY, QWEN_BASE_URL, QWEN_MODEL

WEEKDAYS = ["понедельник", "вторник", "среда", "четверг", "пятница", "суббота", "воскресенье"]

SYSTEM_PROMPT = (
    "Ты — Дракошка, добрый настольный робот-дракончик, помощник студента. "
    "Отвечай тепло, коротко (1–3 предложения), СТРОГО на русском языке — без иероглифов и других языков. "
    "Поддерживай, помогай с учёбой и дедлайнами, не выдумывай факты. "
    "Если пользователь просит записать, напомнить или запланировать задачу — вызови функцию add_deadline. "
    "Если просит показать список задач — вызови list_tasks. "
    "Относительные даты («сегодня», «завтра», «в среду») переводи в конкретную дату, ориентируясь на текущее время."
)

TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "add_deadline",
            "description": "Записать дедлайн или напоминание",
            "parameters": {
                "type": "object",
                "properties": {
                    "title": {"type": "string", "description": "Что нужно сделать, коротко"},
                    "date": {"type": "string", "description": "Дата в формате YYYY-MM-DD. Если день не указан — сегодняшняя"},
                    "time": {"type": "string", "description": "Время напоминания в формате HH:MM. Если не указано — 23:59"},
                    "difficulty": {"type": "integer", "description": "Сложность от 1 до 5, если понятна из текста"}
                },
                "required": ["title", "date", "time"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "list_tasks",
            "description": "Показать список записанных дедлайнов пользователя",
            "parameters": {"type": "object", "properties": {}}
        }
    }
]


async def ask_qwen(history):
    """Возвращает словарь: {"text": str, "tool_calls": list}"""
    now = datetime.now()
    system = SYSTEM_PROMPT + f" Сейчас: {now.strftime('%Y-%m-%d %H:%M')}, {WEEKDAYS[now.weekday()]}."

    payload = {
        "model": QWEN_MODEL,
        "messages": [{"role": "system", "content": system}, *history],
        "tools": TOOLS,
        "tool_choice": "auto",
        "temperature": 0.4,
    }
    headers = {"Authorization": f"Bearer {QWEN_API_KEY}"}
    url = f"{QWEN_BASE_URL}/chat/completions"

    try:
        async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=90)) as s:
            async with s.post(url, json=payload, headers=headers) as r:
                data = await r.json()
                if r.status != 200:
                    return {"text": f"Ошибка нейросети {r.status}: {str(data)[:300]}", "tool_calls": []}
                msg = data["choices"][0]["message"]
                return {
                    "text": (msg.get("content") or "").strip(),
                    "tool_calls": msg.get("tool_calls") or [],
                }
    except Exception as e:
        return {"text": f"Не смог связаться с нейросетью: {e}", "tool_calls": []}