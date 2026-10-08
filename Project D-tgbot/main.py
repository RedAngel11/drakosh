# main.py
import asyncio
import logging
import aiohttp
from aiogram import Bot, Dispatcher, types
from aiogram.filters import CommandStart, Command

import config
from ai_client import get_drakosh_command

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

bot = Bot(token=config.BOT_TOKEN)
dp = Dispatcher()


async def send_command_to_esp32(command_data: dict) -> bool:
    """Асинхронная отправка команды на ESP32 через Cloudflare Relay"""
    cmd_str = command_data.get("command", "unknown")

    for attempt in range(config.MAX_RETRIES):
        try:
            async with aiohttp.ClientSession() as session:
                # Формируем URL как в твоем старом коде: /box/push?cmd=dance
                url = f"{config.RELAY_URL}/box/push?cmd={cmd_str}"

                logger.info(f"📡 Отправка команды через реле: {url}")

                async with session.get(url, timeout=config.AI_TIMEOUT) as response:
                    if response.status == 200:
                        logger.info(f"✅ Команда '{cmd_str}' успешно доставлена через реле!")
                        return True
                    else:
                        logger.warning(f"⚠️ Реле вернуло статус {response.status}")

        except asyncio.TimeoutError:
            logger.warning(f"⚠️ Попытка {attempt + 1}: Таймаут соединения с реле")
        except aiohttp.ClientError as e:
            logger.error(f"❌ Ошибка сети при отправке: {e}")

        if attempt < config.MAX_RETRIES - 1:
            await asyncio.sleep(1.5)  # Асинхронная пауза, не блокирует бота!

    logger.error("❌ Все попытки отправки команды через реле провалились")
    return False


@dp.message(CommandStart())
async def cmd_start(message: types.Message):
    welcome_text = (
        "🐉 Рррр! Привет! Я твой дракошка!\n\n"
        "Я подключен через надежное облачное реле.\n"
        "Напиши: 'привет', 'станцуй', 'радуга' или 'поменяй цвет на синий'!"
    )
    await message.answer(welcome_text)


@dp.message(Command("status"))
async def cmd_status(message: types.Message):
    """Проверка статуса через реле"""
    try:
        async with aiohttp.ClientSession() as session:
            # Предполагаем, что у реле есть эндпоинт /status или /box/status
            url = f"{config.RELAY_URL}/box/status"
            async with session.get(url, timeout=5) as response:
                if response.status == 200:
                    data = await response.json()
                    await message.answer(f"✅ Дракошка онлайн! Статус: {data.get('status', 'OK')}")
                else:
                    await message.answer("⚠️ Реле работает, но дракошка не отвечает.")
    except Exception as e:
        await message.answer(f"❌ Не удалось связаться с реле: {e}")


@dp.message()
async def handle_user_message(message: types.Message):
    user_text = message.text.strip()
    if not user_text:
        return

    logger.info(f"📝 Получено сообщение: {user_text}")

    thinking_msg = await message.answer("🤔 Дракошка думает...")

    # Получаем команду от нейросети
    command_data = get_drakosh_command(user_text)

    try:
        await thinking_msg.delete()
    except Exception:
        pass

    reply_text = command_data.get("reply_to_user", "Рррр?")
    await message.answer(reply_text)

    command = command_data.get("command", "unknown")

    if command != "unknown":
        success = await send_command_to_esp32(command_data)
        if not success:
            await message.answer(
                "⚠️ Я поняла тебя, но не смогла достучаться до дракошки. "
                "Возможно, он спит или потерял связь с интернетом."
            )
    else:
        logger.info("Неизвестная команда, не отправляем на ESP32")


async def main():
    logger.info("🚀 Запуск бота Drakoshka (Режим: Cloudflare Relay)...")
    await dp.start_polling(bot)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("🛑 Бот остановлен пользователем")