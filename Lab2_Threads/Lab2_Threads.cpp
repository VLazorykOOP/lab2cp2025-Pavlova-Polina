#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <cmath>
#include <Windows.h>

using namespace std;
using namespace std::chrono;

// Клас для консольної графіки
class ConsoleGraphics {
private:
    HANDLE hConsole;
    COORD bufferSize;
    CHAR_INFO* buffer;
    mutex consoleMutex;

public:
    ConsoleGraphics(int width, int height) {
        hConsole = CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CONSOLE_TEXTMODE_BUFFER,
            NULL
        );

        SetConsoleActiveScreenBuffer(hConsole);

        bufferSize.X = width;
        bufferSize.Y = height;

        buffer = new CHAR_INFO[width * height];
        for (int i = 0; i < width * height; i++) {
            buffer[i].Char.AsciiChar = ' ';
            buffer[i].Attributes = 0;
        }
    }

    ~ConsoleGraphics() {
        CloseHandle(hConsole);
        delete[] buffer;
    }

    void clear() {
        lock_guard<mutex> lock(consoleMutex);
        for (int i = 0; i < bufferSize.X * bufferSize.Y; i++) {
            buffer[i].Char.AsciiChar = ' ';
            buffer[i].Attributes = 0;
        }
    }

    void setPixel(int x, int y, char c, int color) {
        lock_guard<mutex> lock(consoleMutex);
        if (x >= 0 && x < bufferSize.X && y >= 0 && y < bufferSize.Y) {
            int index = y * bufferSize.X + x;
            buffer[index].Char.UnicodeChar = c; // Змінено з AsciiChar на UnicodeChar
            buffer[index].Attributes = color;
        }
    }

    void render() {
        lock_guard<mutex> lock(consoleMutex);
        SMALL_RECT writeArea = { 0, 0, bufferSize.X - 1, bufferSize.Y - 1 };
        WriteConsoleOutput(hConsole, buffer, bufferSize, { 0, 0 }, &writeArea);
    }

    int getWidth() const { return bufferSize.X; }
    int getHeight() const { return bufferSize.Y; }
};

// Базовий клас для всіх комах
class Insect {
protected:
    double x, y;          // Поточна позиція
    double birthX, birthY; // Місце народження
    double velocity;      // Швидкість руху
    char symbol;          // Символ для відображення
    int color;            // Колір
    bool active;          // Активність комахи

public:
    Insect(double startX, double startY, double vel, char sym, int col)
        : x(startX), y(startY), birthX(startX), birthY(startY),
        velocity(vel), symbol(sym), color(col), active(true) {
    }

    virtual void move(double deltaTime) = 0; // Абстрактний метод руху

    void draw(ConsoleGraphics& graphics) {
        graphics.setPixel(static_cast<int>(x), static_cast<int>(y), symbol, color);
    }

    bool isActive() const { return active; }
    void setActive(bool isActive) { active = isActive; }

    double getX() const { return x; }
    double getY() const { return y; }
};

// Клас бджоли-робочої
class WorkerBee : public Insect {
private:
    bool goingToCorner; // Напрямок руху: до кута чи назад
    double targetX, targetY; // Цільова точка

public:
    WorkerBee(double startX, double startY, double vel)
        : Insect(startX, startY, vel, 'B', 14), // Жовтий колір
        goingToCorner(true), targetX(0), targetY(0) {
    }

    void move(double deltaTime) override {
        if (!active) return;

        if (goingToCorner) {
            // Рух до кута [0,0]
            double dx = targetX - x;
            double dy = targetY - y;
            double distance = sqrt(dx * dx + dy * dy);

            // Якщо прибули до кута
            if (distance < velocity * deltaTime) {
                x = targetX;
                y = targetY;
                goingToCorner = false; // Змінюємо напрямок
            }
            else {
                // Рухаємось до кута
                double ratio = velocity * deltaTime / distance;
                x += dx * ratio;
                y += dy * ratio;
            }
        }
        else {
            // Рух назад до місця народження
            double dx = birthX - x;
            double dy = birthY - y;
            double distance = sqrt(dx * dx + dy * dy);

            // Якщо прибули додому
            if (distance < velocity * deltaTime) {
                x = birthX;
                y = birthY;
                goingToCorner = true; // Змінюємо напрямок
            }
            else {
                // Рухаємось до місця народження
                double ratio = velocity * deltaTime / distance;
                x += dx * ratio;
                y += dy * ratio;
            }
        }
    }
};

// Клас трутня
class Drone : public Insect {
private:
    double directionX, directionY; // Напрямок руху
    double timeToChangeDirection;  // Час до зміни напрямку
    double changeDirectionInterval; // Інтервал зміни напрямку
    std::mt19937 rng;               // Генератор випадкових чисел

public:
    Drone(double startX, double startY, double vel, double changeInterval)
        : Insect(startX, startY, vel, 'T', 12), // Червоний колір
        timeToChangeDirection(changeInterval),
        changeDirectionInterval(changeInterval) {

        // Ініціалізація генератора випадкових чисел
        random_device rd;
        rng = std::mt19937(rd());

        // Початковий випадковий напрямок
        setRandomDirection();
    }

    void setRandomDirection() {
        // Розподіл кута від 0 до 2π
        std::uniform_real_distribution<double> angleDist(0, 2 * 3.14159265358979323846);
        double angle = angleDist(rng);

        // Перетворення кута в напрямок
        directionX = cos(angle);
        directionY = sin(angle);
    }

    void move(double deltaTime) override {
        if (!active) return;

        // Зменшуємо час до зміни напрямку
        timeToChangeDirection -= deltaTime;
        if (timeToChangeDirection <= 0) {
            setRandomDirection();
            timeToChangeDirection = changeDirectionInterval;
        }

        // Рухаємося у поточному напрямку
        x += directionX * velocity * deltaTime;
        y += directionY * velocity * deltaTime;

        // Обмеження руху в межах екрану
        if (x < 0) { x = 0; directionX = -directionX; }
        if (y < 0) { y = 0; directionY = -directionY; }
        if (x >= 80) { x = 79; directionX = -directionX; }
        if (y >= 25) { y = 24; directionY = -directionY; }
    }
};

// Функція для потоку комахи
void insectThread(Insect* insect, ConsoleGraphics& graphics, atomic<bool>& running, double velocity) {
    // Визначаємо пріоритет потоку (для демонстрації)
    int priority = (insect->getX() + insect->getY()) > 50 ?
        THREAD_PRIORITY_ABOVE_NORMAL : THREAD_PRIORITY_NORMAL;

    // Встановлюємо пріоритет поточного потоку
    SetThreadPriority(GetCurrentThread(), priority);

    while (running) {
        insect->move(0.1); // Рух з часовим кроком 0.1 секунди
        this_thread::sleep_for(milliseconds(static_cast<int>(100 / velocity))); // Коригуємо швидкість
    }
}

int main() {
    // Встановлюємо розмір консолі
    system("mode con cols=80 lines=30");

    // Створюємо графічний буфер
    ConsoleGraphics graphics(80, 25);

    // Кількість комах
    const int workerBeesCount = 5;
    const int dronesCount = 3;

    // Швидкість та інтервал зміни напрямку
    double beeVelocity = 2.0;  // Швидкість бджіл
    double droneVelocity = 2.0; // Швидкість трутнів
    double changeInterval = 7.0; // Інтервал зміни напрямку для трутнів (секунди)

    // Вектори для зберігання комах та потоків
    vector<unique_ptr<Insect>> insects;
    vector<thread> threads;

    // Прапорець для завершення всіх потоків
    atomic<bool> running(true);

    // Генератор випадкових чисел для початкових позицій
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distX(10, 70);
    uniform_int_distribution<> distY(5, 20);

    // Створюємо бджіл
    for (int i = 0; i < workerBeesCount; ++i) {
        insects.push_back(make_unique<WorkerBee>(distX(gen), distY(gen), beeVelocity));
    }

    // Створюємо трутнів
    for (int i = 0; i < dronesCount; ++i) {
        insects.push_back(make_unique<Drone>(distX(gen), distY(gen), droneVelocity, changeInterval));
    }

    // Створюємо потоки для кожної комахи
    for (auto& insect : insects) {
        threads.emplace_back(insectThread, insect.get(), ref(graphics), ref(running),
            (dynamic_cast<WorkerBee*>(insect.get()) ? beeVelocity : droneVelocity));
    }

    // Основний цикл візуалізації
    cout << "Simulation running. Press ESC to exit." << endl;
    cout << "B - Worker Bee (Yellow)" << endl;
    cout << "T - Drone (Red)" << endl;

    while (true) {
        // Перевіряємо, чи натиснута клавіша ESC
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            running = false;
            break;
        }

        // Очищаємо буфер
        graphics.clear();

        // Малюємо комах
        for (auto& insect : insects) {
            insect->draw(graphics);
        }

        // Виводимо на екран
        graphics.render();

        // Пауза для стабільного FPS
        this_thread::sleep_for(milliseconds(50));
    }

    // Чекаємо завершення всіх потоків
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Завершення програми
    system("cls");
    cout << "Simulation ended." << endl;

    return 0;
}