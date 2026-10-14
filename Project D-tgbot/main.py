import asyncio
import json
import os
import re
from datetime import datetime
import aiohttp
from aiogram import Bot, Dispatcher, types
from aiogram.filters import CommandStart, Command
from aiogram.client.session.aiohttp import AiohttpSession
from aiogram.client.telegram import TelegramAPIServer

from config import TOKEN, RELAY_URL
from ai_client import get_drakosh_command

# === Обход блокировки через Cloudflare ===
server = TelegramAPIServer.from_base(RELAY_URL)
session = AiohttpSession(api=server)
bot = Bot(token=TOKEN, session=session)
dp = Dispatcher()

# === Хранилище дедлайнов ===
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

def deadlines_text(chat_id):
    my = sorted((d for d in DEADLINES if d["chat"] == chat_id),
                key=lambda d: (d.get("date") or "", d["time"]))
    if not my:
        return "Пока пусто. Скажи: «напомни завтра в 10:00 сдать лабу»"
    today = datetime.now().strftime("%Y-%m-%d")
    lines = []
    for i, d in enumerate(my):
        when = "сегодня" if (d.get("date") or today) == today else d.get("date", "")
        lines.append(f"{i+1}. ⏰ {when} {d['time']} — {d['text']}")
    return "Твои дедлайны:\n" + "\n".join(lines)

async def push_cmd(cmd: str):
    try:
        async with aiohttp.ClientSession() as s:
            await s.get(f"{RELAY_URL}/box/push", params={"cmd": cmd}, timeout=5)
    except Exception as e:
        print("Не удалось отправить команду роботу:", e)

async def reminder_loop():
    """Фоновая задача — проверяет дедлайны каждые 15 секунд"""
    while True:
        now = datetime.now()
        now_date = now.strftime("%Y-%m-%d")
        now_time = now.strftime("%H:%M")
        fired = False
        for d in list(DEADLINES):
            if d["time"] == now_time and (d.get("date") is None or d.get("date") == now_date):
                try:
                    await bot.send_message(d["chat"], f"🔥 Дедлайн: {d['text']}! Дракошка верит в тебя 🐉")
                    # Дополнительно: робот подаёт сигнал тревоги
                    await push_cmd("light_alarm")
                except Exception as e:
                    print("Ошибка напоминания:", e)
                DEADLINES.remove(d)
                fired = True
        if fired:
            save_deadlines()
        await asyncio.sleep(15)

@dp.message(CommandStart())
async def cmd_start(message: types.Message):
    await message.answer(
        "🐉 Рррр! Привет! Я твой дракошка!\n\n"
        "Я могу:\n"
        "• Управлять светом и танцевать\n"
        "• Запоминать дедлайны и напоминать о них\n"
        "• Просто болтать и поддерживать\n\n"
        "Попробуй: «привет», «станцуй», «напомни завтра в 18:00 сдать курсач»"
    )

@dp.message(Command("deadlines"))
async def show_deadlines(message: types.Message):
    await message.answer(deadlines_text(message.chat.id))

@dp.message()
async def handle_user_message(message: types.Message):
    user_text = message.text.strip()
    if not user_text:
        return

    thinking_msg = await message.answer("🤔 Дракошка думает...")

    try:
        command_data = await asyncio.to_thread(get_drakosh_command, user_text)
    except Exception as e:
        print("Ошибка нейросети:", e)
        command_data = {"command": "unknown", "params": {},
                        "reply_to_user": "Рррр? Я отвлёкся, повтори!"}

    try:
        await thinking_msg.delete()
    except Exception:
        pass

    reply_text = command_data.get("reply_to_user", "Рррр?")
    cmd = command_data.get("command", "unknown")
    params = command_data.get("params") or {}

    # === Обработка команд дедлайнов ===
    if cmd == "add_deadline":
        DEADLINES.append({
            "chat": message.chat.id,
            "date": params.get("date") or datetime.now().strftime("%Y-%m-%d"),
            "time": params.get("time") or "23:59",
            "text": params.get("title") or "задача",
            "difficulty": params.get("difficulty"),
        })
        DEADLINES.sort(key=lambda d: (d.get("date") or "", d["time"]))
        save_deadlines()

    elif cmd == "list_deadlines":
        reply_text = deadlines_text(message.chat.id)

    # === Отправка ответа ===
    for attempt in range(2):
        try:
            await message.answer(reply_text)
            break
        except Exception as e:
            print(f"Не удалось отправить ответ (попытка {attempt+1}): {e}")
            await asyncio.sleep(2)

    # === Команда роботу ===
    if cmd not in ("unknown", "add_deadline", "list_deadlines"):
        # Сборка команды с параметром (для change_color и пр.)
        cmd_str = cmd
        if cmd == "change_color" and params.get("color"):
            cmd_str = f"change_color:{params['color']}"
        elif cmd == "set_brightness" and params.get("value") is not None:
            cmd_str = f"set_brightness:{int(params['value'])}"
        await push_cmd(cmd_str)


async def main():
    asyncio.create_task(reminder_loop())
    await dp.start_polling(bot)

if __name__ == "__main__":
    asyncio.run(main())