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
from qwen_client import ask_qwen

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
CHAT_HISTORY = {}  # память диалогов с нейросетью

def parse_time(s):
    """Понимает 24-часовой формат: '18:05', '8:05', '18.05'"""
    m = re.match(r"^(\d{1,2})[:.](\d{2})$", s.strip())
    if not m:
        return None
    h, mi = int(m.group(1)), int(m.group(2))
    if h > 23 or mi > 59:
        return None
    return f"{h:02d}:{mi:02d}"   # приводим к '08:05' — так сортируется правильно
def parse_date(s):
    try:
        return datetime.strptime((s or "").strip(), "%Y-%m-%d").strftime("%Y-%m-%d")
    except Exception:
        return None


def deadlines_text(chat_id):
    my = sorted(
        (d for d in DEADLINES if d["chat"] == chat_id),
        key=lambda d: (d.get("date") or "", d["time"])
    )
    if not my:
        return "Пока пусто. Скажи просто: «запиши, что завтра в 10:00 сдать лабу»"

    today = datetime.now().strftime("%Y-%m-%d")
    lines = []
    for i, d in enumerate(my):
        when = "сегодня" if (d.get("date") or today) == today else d.get("date", "")
        lines.append(f"{i + 1}. ⏰ {when} {d['time']} — {d['text']}")
    return "Твои дедлайны:\n" + "\n".join(lines)
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
    await m.answer(deadlines_text(m.chat.id))

async def reminder_loop():
    while True:
        now = datetime.now()
        now_date = now.strftime("%Y-%m-%d")
        now_time = now.strftime("%H:%M")
        fired = False

        for d in list(DEADLINES):
            time_match = d["time"] == now_time
            date_ok = (d.get("date") is None) or (d.get("date") == now_date)

            if time_match and date_ok:
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
    t = (m.text or "").lower()

    # ===== НОВЫЕ КОМАНДЫ НАСТРОЕНИЯ =====
    if "посвети" in t or "радость" in t or "весело" in t or "joy" in t:
        await push_cmd("light_joy")
        await m.answer("Свечусь радостью! 🟠🐉")
        return

    if "погасни" in t or "выключи" in t or "off" in t:
        await push_cmd("light_off")
        await m.answer("Гасну... 🌑")
        return

    if "спокой" in t or "тихо" in t or "calm" in t or "расслаб" in t:
        await push_cmd("light_calm")
        await m.answer("Спокойствие и гармония 🔵")
        return

    if "поддерж" in t or "обним" in t or "груст" in t or "support" in t:
        await push_cmd("light_support")
        await m.answer("Обнимаю тебя! 💚🐉")
        return

    if "дедлайн" in t or "тревог" in t or "panic" in t or "alarm" in t or "срочн" in t:
        await push_cmd("light_alarm")
        await m.answer("Внимание! Режим тревоги! 🔴")
        return

    if "сон" in t or "спать" in t or "sleep" in t or "ночь" in t:
        await push_cmd("light_sleep")
        await m.answer("Спокойной ночи... 💜")
        return

    history = CHAT_HISTORY.get(m.chat.id, [])[-10:]
    history.append({"role": "user", "content": m.text})

    result = await ask_qwen(history)
    replies = []

    for call in result["tool_calls"]:
        fname = call.get("function", {}).get("name")
        try:
            args = json.loads(call.get("function", {}).get("arguments") or "{}")
        except json.JSONDecodeError:
            args = {}

        if fname == "add_deadline":
            date = parse_date(args.get("date")) or datetime.now().strftime("%Y-%m-%d")
            time_ = parse_time(args.get("time") or "") or "23:59"
            title = (args.get("title") or "задача").strip()
            diff = args.get("difficulty")

            DEADLINES.append({
                "chat": m.chat.id,
                "date": date,
                "time": time_,
                "text": title,
                "difficulty": diff,
            })
            DEADLINES.sort(key=lambda d: (d.get("date") or "", d["time"]))
            save_deadlines()

            pretty = "сегодня" if date == datetime.now().strftime("%Y-%m-%d") else date
            extra = f", сложность {diff}/5" if diff else ""
            replies.append(f"Записал! Напомню {pretty} в {time_}: «{title}»{extra} ⏰")

        elif fname == "list_tasks":
            replies.append(deadlines_text(m.chat.id))

    if result["text"]:
        replies.append(result["text"])

    history.append({"role": "assistant", "content": result["text"] or "\n".join(replies)})
    CHAT_HISTORY[m.chat.id] = history

    await m.answer(("\n".join(replies) or "Понял!")[:4096])
async def main():
    dp.include_router(router)
    asyncio.create_task(reminder_loop())
    await dp.start_polling(bot, polling_timeout=10)

if __name__ == "__main__":
    asyncio.run(main())