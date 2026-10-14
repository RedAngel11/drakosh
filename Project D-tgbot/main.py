import asyncio
import aiohttp
from aiogram import Bot, Dispatcher, Router, types
from aiogram.filters import CommandStart
from aiogram.client.session.aiohttp import AiohttpSession
from aiogram.client.telegram import TelegramAPIServer
from aiogram import F

from config import TOKEN, RELAY_URL
from ai_client import get_drakosh_command

# ВОЗВРАЩАЕМ РАБОЧИЙ ПРОКСИ ЧЕРЕЗ CLOUDFLARE
server = TelegramAPIServer.from_base(RELAY_URL)
session = AiohttpSession(api=server)
bot = Bot(token=TOKEN, session=session)

dp = Dispatcher()
router = Router()

def build_cmd(command_data: dict) -> str:
    """Склеивает команду и параметр в одну строку для реле"""
    cmd = command_data.get("command", "unknown")
    params = command_data.get("params") or {}
    if cmd == "change_color" and params.get("color"):
        return f"change_color:{params['color']}"
    if cmd == "set_brightness" and params.get("value") is not None:
        return f"set_brightness:{int(params['value'])}"
    return cmd

async def push_cmd(cmd: str):
    try:
        async with aiohttp.ClientSession() as s:
            await s.get(f"{RELAY_URL}/box/push", params={"cmd": cmd}, timeout=5)
    except Exception as e:
        print("Не удалось отправить команду роботу:", e)


@router.message(CommandStart())
async def start(m: types.Message):
    await m.answer(
        "Привет! Я Дракошка 🐉\n"
        "Напиши мне что-нибудь, например:\n"
        "• 'привет' или 'станцуй'\n"
        "• 'поменяй цвет на синий'\n"
        "• или просто расскажи, как прошел день!"
    )


@router.message(F.text)
async def free_text(m: types.Message):
    user_text = m.text.strip()
    if not user_text:
        return

    thinking_msg = None
    try:
        thinking_msg = await m.answer("🤔 Дракошка думает...")
    except Exception:
        pass

    # ГЛАВНАЯ ПРАВКА: синхронный запрос к Яндексу уходит в отдельный поток,
    # event loop больше не блокируется, бот не "замолкает"
    try:
        command_data = await asyncio.to_thread(get_drakosh_command, user_text)
    except Exception as e:
        print("Ошибка нейросети:", e)
        command_data = {"command": "unknown", "params": {},
                        "reply_to_user": "Рррр? Я отвлёкся, повтори, пожалуйста!"}

    reply_text = command_data.get("reply_to_user", "Рррр?")

    # ПРАВКА 2: отправка ответа с одной повторной попыткой при таймауте
    for attempt in range(2):
        try:
            await m.answer(reply_text)
            break
        except Exception as e:
            print(f"⚠️ Не удалось отправить ответ (попытка {attempt + 1}): {e}")
            await asyncio.sleep(2)

    # ПРАВКА 3: команда на робота не должна ронять обработчик
    command = command_data.get("command", "unknown")
    if command != "unknown":
        try:
            await push_cmd(build_cmd(command_data))
        except Exception as e:
            print("Не удалось отправить команду роботу:", e)

    if thinking_msg:
        try:
            await thinking_msg.delete()
        except Exception:
            pass


async def main():
    dp.include_router(router)
    await dp.start_polling(bot, polling_timeout=10)


if __name__ == "__main__":
    asyncio.run(main())