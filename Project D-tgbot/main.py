import asyncio
import json
import os
import re
from datetime import datetime

from aiogram import Bot, Dispatcher, Router, types
from aiogram.filters import Command, CommandStart
from aiogram.client.session.aiohttp import AiohttpSession
from aiogram.client.telegram import TelegramAPIServer

from config import TOKEN

# Ходим через наше реле — VPN не нужен
server = TelegramAPIServer.from_base("https://helloesp32.ksushat75.workers.dev/")
session = AiohttpSession(api=server)
bot = Bot(token=TOKEN, session=session)

dp = Dispatcher()
router = Router()

# --- Память дедлайнов: файл рядом с ботом ---
DB_FILE = os.path.join(os.path.dirname(__file__), "deadlines.json")

def load_deadlines():
    try:
        with open(DB_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return []

def save_deadlines():
    with open(DB_FILE, "w", encoding="utf-8") as f:
        json.dump(DEADLINES, f, ensure_ascii=False, indent=2)

DEADLINES = load_deadlines()

def parse_time(s):
    """Понимает 24-часовой формат: '18:05', '8:05', '18.05'"""
    m = re.match(r"^(\d{1,2})[:.](\d{2})$", s.strip())
    if not m:
        return None
    h, mi = int(m.group(1)), int(m.group(2))
    if h > 23 or mi > 59:
        return None
    return f"{h:02d}:{mi:02d}"   # приводим к '08:05' — так сортируется правильно

@router.message(CommandStart())
async def start(m: types.Message):
    await m.answer(
        "Привет! Я Дракошка 🐉\n"
        "/set_deadline 18:00 сдать лабу — добавить дедлайн\n"
        "/deadline — показать список по возрастанию времени"
    )

@router.message(Command("set_deadline"))
async def set_deadline(m: types.Message):
    parts = (m.text or "").split(maxsplit=2)
    if len(parts) < 3:
        await m.answer("Формат: /set_deadline 18:00 сдать лабу")
        return
    _, raw_time, text = parts
    t = parse_time(raw_time)
    if t is None:
        await m.answer("Не понял время 🙈 Нужен 24-часовой формат: /set_deadline 18:00 сдать лабу")
        return
    DEADLINES.append({"chat": m.chat.id, "time": t, "text": text})
    DEADLINES.sort(key=lambda d: d["time"])
    save_deadlines()
    await m.answer(f"Записал! В {t} напомню: «{text}» ⏰")

@router.message(Command("deadline"))
async def show_deadlines(m: types.Message):
    my = sorted((d for d in DEADLINES if d["chat"] == m.chat.id), key=lambda d: d["time"])
    if not my:
        await m.answer("Пока пусто. Добавь: /set_deadline 18:00 сдать лабу")
        return
    lines = [f"{i + 1}. ⏰ {d['time']} — {d['text']}" for i, d in enumerate(my)]
    await m.answer("Твои дедлайны:\n" + "\n".join(lines))

async def reminder_loop():
    while True:
        now = datetime.now().strftime("%H:%M")
        fired = False
        for d in list(DEADLINES):
            if d["time"] == now:
                try:
                    await bot.send_message(d["chat"], f"🔥 Дедлайн: {d['text']}! Дракошка верит в тебя 🐉")
                except Exception as e:
                    print("Ошибка напоминания:", e)
                DEADLINES.remove(d)
                fired = True
        if fired:
            save_deadlines()
        await asyncio.sleep(15)

import aiohttp
from aiogram import F

RELAY = "https://helloesp32.ksushat75.workers.dev"

async def push_cmd(cmd: str):
    try:
        async with aiohttp.ClientSession() as s:
            await s.get(f"{RELAY}/box/push", params={"cmd": cmd}, timeout=5)
    except Exception as e:
        print("Не удалось отправить команду роботу:", e)

@router.message(F.text)
async def free_text(m: types.Message):
    t = m.text.lower()
    if "посвети" in t:
        await push_cmd("light_on")
        await m.answer("Свечусь!")
    elif "погасни" in t:
        await push_cmd("light_off")
        await m.answer("Гасну")

async def main():
    dp.include_router(router)
    asyncio.create_task(reminder_loop())
    await dp.start_polling(bot, polling_timeout=10)

if __name__ == "__main__":
    asyncio.run(main())