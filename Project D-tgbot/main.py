"""
Telegram-бот для управления роботом-драконом Drakoshka
"""

import asyncio
import logging
import requests
from aiogram import Bot, Dispatcher, types
from aiogram.filters import CommandStart, Command

import config
from qwen_client import get_drakosh_command

# Настройка логирования
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Инициализация бота
bot = Bot(token=config.BOT_TOKEN)
dp = Dispatcher()


def send_command_to_esp32(command_data: dict) -> bool:
    """
    Отправка команды на ESP32 через HTTP POST
    Возвращает True если команда доставлена успешно
    """
    for attempt in range(config.MAX_RETRIES):
        try:
            response = requests.post(
                config.ESP32_COMMAND_URL,
                json=command_data,
                timeout=config.ESP32_TIMEOUT
            )
            response.raise_for_status()
            logger.info(f"✅ Команда отправлена на ESP32: {command_data['command']}")
            return True
        except requests.exceptions.ConnectionError:
            logger.warning(f"⚠️ Попытка {attempt + 1}: ESP32 недоступен")
            if attempt < config.MAX_RETRIES - 1:
                asyncio.sleep(1)
        except requests.exceptions.RequestException as e:
            logger.error(f"❌ Ошибка отправки команды: {e}")
            return False

    logger.error("❌ Все попытки отправки команды на ESP32 провалились")
    return False


@dp.message(CommandStart())
async def cmd_start(message: types.Message):
    """Обработчик команды /start"""
    welcome_text = (
        "🐉 Рррр! Привет! Я твой дракошка Дракошка!\n\n"
        "Вот что я умею:\n"
        "• Поздороваться и попрощаться\n"
        "• Петь песенки\n"
        "• Танцевать\n"
        "• Включать радугу\n"
        "• Менять цвет\n"
        "• Регулировать яркость, громкость\n\n"
        "Просто напиши мне, что хочешь!"
    )
    await message.answer(welcome_text)


@dp.message(Command("help"))
async def cmd_help(message: types.Message):
    """Обработчик команды /help"""
    help_text = (
        " Доступные команды:\n\n"
        "• привет / hello — поздороваться\n"
        "• пока / bye — попрощаться\n"
        "• спой / sing — спеть песенку\n"
        "• станцуй / dance — потанцевать\n"
        "• радуга / rainbow — включить радугу\n"
        "• поменяй цвет на [цвет] — сменить цвет\n"
        "  (red, blue, green, yellow, purple, white)\n"
        "• яркость [0-255] — изменить яркость\n"
        "• громкость [0-100] — изменить громкость\n"
        "• моргни — моргнуть глазами\n\n"
        "Или просто напиши что-нибудь, я пойму!"
    )
    await message.answer(help_text)


@dp.message(Command("status"))
async def cmd_status(message: types.Message):
    """Проверка статуса ESP32"""
    try:
        response = requests.get(
            f"http://{config.ESP32_IP}:{config.ESP32_PORT}/status",
            timeout=3
        )
        if response.status_code == 200:
            await message.answer("✅ Дракошка онлайн и готов к командам!")
        else:
            await message.answer("⚠️ Дракошка отвечает, но что-то не так...")
    except requests.exceptions.RequestException:
        await message.answer("❌ Дракошка offline. Проверь подключение к сети.")


@dp.message()
async def handle_user_message(message: types.Message):
    """Обработчик всех текстовых сообщений"""
    user_text = message.text.strip()

    if not user_text:
        return

    # Отправляем сообщение в нейросеть
    logger.info(f"📝 Получено сообщение: {user_text}")

    # Показываем индикатор "думаю"
    thinking_msg = await message.answer("🤔 Дракошка думает...")

    # Получаем команду от нейросети
    command_data = get_drakosh_command(user_text)

    # Удаляем сообщение "думаю"
    try:
        await thinking_msg.delete()
    except:
        pass

    # Отправляем текстовый ответ пользователю
    reply_text = command_data.get("reply_to_user", "Рррр?")
    await message.answer(reply_text)

    # Отправляем команду на ESP32
    command = command_data.get("command", "unknown")

    if command != "unknown":
        success = send_command_to_esp32(command_data)
        if not success:
            await message.answer(
                "⚠️ Команду поняла, но не смогла передать дракошке. "
                "Проверь, что он включён и подключён к сети."
            )
    else:
        logger.info(f"Неизвестная команда, не отправляем на ESP32")


async def main():
    """Запуск бота"""
    logger.info("🚀 Запуск бота Drakoshka...")
    logger.info(f"🤖 AI провайдер: {config.AI_PROVIDER}")
    logger.info(f" ESP32 адрес: {config.ESP32_IP}")

    await dp.start_polling(bot)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info(" Бот остановлен пользователем")