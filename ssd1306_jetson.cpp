#include <iostream>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>
#include <sstream> // Required for std::istringstream


#define I2C_BUS "/dev/i2c-8"
#define OLED_ADDR 0x3C


// Function to read CPU usage per core
std::vector<int> getCPUUsage() {
    std::ifstream file("/proc/stat");
    std::vector<int> cpu_usage;
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.rfind("cpu", 0) == 0 && line[3] != ' ') { // Ignore "cpu" (total), only "cpu0", "cpu1", etc.
            std::istringstream iss(line);
            std::string cpu;
            int user, nice, system, idle, iowait, irq, softirq, steal;
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            int active_time = user + nice + system + irq + softirq + steal;
            int total_time = active_time + idle + iowait;
            int usage = (total_time == 0) ? 0 : (100 * active_time) / total_time;
            cpu_usage.push_back(usage);
        }
    }

    return cpu_usage;
}


int getGPUUsage() {
    std::ifstream file("/sys/devices/gpu.0/load");
    int gpu_usage = 0;
    if (file.is_open()) {
        file >> gpu_usage;
    }
    return gpu_usage / 10; // Convert from 0-1000 range to 0-100%
}

// Function to map percentage to pixel height (64 pixels max)
int mapToHeight(int percentage) {
    return (percentage * 64) / 100; // Scale 0-100% to 0-64 pixels
}


void clearDisplayFast();

// ✅ Move resetI2CBus() here, outside the class
void resetI2CBus() {
    std::cout << "Resetting I2C Bus...\n";
    system("sudo i2cset -y 8 0x3C 0x00 0xAA");  // Dummy command to reset
    system("sudo i2cset -y 8 0x3C 0x00 0xAF");  // Try turning the display ON
    system("sudo i2cset -y 8 0x3C 0x00 0xAE");  // Try turning the display OFF
    std::cout << "I2C Bus-Speed: ";
    system("sudo cat /sys/kernel/debug/bpmp/debug/clk/i2c8/rate");
    usleep(10000); // Wait 10ms
    std::cout << "Reset I2C Bus OK!\n";
}

class SSD1306 {
private:
    int file;

public:

    void clearDisplay();  // ✅ Declare inside the class

    SSD1306() {
        file = open(I2C_BUS, O_RDWR);
        if (file < 0) {
            std::cerr << "Failed to open I2C bus: " << strerror(errno) << "\n";
            exit(1);
        }

        if (ioctl(file, I2C_SLAVE, OLED_ADDR) < 0) {
            std::cerr << "Failed to connect to OLED at 0x3C: " << strerror(errno) << "\n";
            exit(1);
        }

        std::cout << "OLED initialized on " << I2C_BUS << " at address 0x3C.\n";
        initialize();
    }

    ~SSD1306() {
        close(file);
    }

    void writeCommand(uint8_t command) {
        //std::cout << "writeCommand\n";
        uint8_t buffer[2] = {0x00, command};
        if (write(file, buffer, 2) != 2) {
            std::cerr << "Error writing command " << int(command) << ": " << strerror(errno) << "\n";
        }
        usleep(1000);
    }



void writeData(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        uint8_t buffer[2] = {0x40, data[i]}; // Control byte + 1 data byte

        if (write(file, buffer, 2) != 2) {
            std::cerr << "Error writing data at index " << i << ": " << strerror(errno) << "\n";
            return;
        }

        usleep(500); // ✅ Small delay to prevent I2C congestion
    }
}




    void initialize() {
        std::cout << "Start initialize...\n";

        usleep(200000); // 200ms delay to stabilize

        writeCommand(0xAE);
        writeCommand(0xD5); writeCommand(0x80);
        writeCommand(0xA8); writeCommand(0x3F);
        writeCommand(0xD3); writeCommand(0x00);
        writeCommand(0x40);
        writeCommand(0x8D); writeCommand(0x14);
        writeCommand(0x20); writeCommand(0x00);  // **Set Horizontal Addressing Mode**
        writeCommand(0xA1);
        writeCommand(0xC8);
        writeCommand(0xDA); writeCommand(0x12);
        writeCommand(0x81); writeCommand(0xCF);
        writeCommand(0xD9); writeCommand(0xF1);
        writeCommand(0xDB); writeCommand(0x40);
        writeCommand(0xA4);
        writeCommand(0xA6);
        writeCommand(0xAF);

        usleep(200000); // **Add a delay after initialization**
        std::cout << "End initialize!\n";
    }



void clearDisplayFast() {
    writeCommand(0xAE); // ✅ Display OFF (prevents glitches)
    //usleep(100);     // ✅ Wait 100ms for SSD1306 to fully power down

    // ✅ Clear the GDDRAM (128x8 pages of zeros)
    uint8_t clearBuffer[1] = {0}; // Single-byte buffer for safe writing
    for (int i = 0; i < 8; i++) { // 8 pages for 64-pixel height
        writeCommand(0xB0 + i); // Set Page Address
        writeCommand(0x00); // Lower Column Start Address
        writeCommand(0x10); // Higher Column Start Address
        
        //usleep(100); // ✅ Short delay before writing data

        for (int j = 0; j < 128; j++) {
            writeData(clearBuffer, 1); // ✅ Write 1 byte at a time
            ///usleep(100);
        }
    }

    //usleep(100); // ✅ Wait 100ms before turning ON again
    writeCommand(0xAF); // ✅ Display ON (screen should now be blank)
}

int mapToHeight(int percentage) {
    int height = (percentage * 64) / 100;
    return (height < 8) ? 8 : height; // ✅ Ensure at least 8 pixels for visibility
}


};

void SSD1306::clearDisplay() {
    uint8_t clearBuffer[128] = {0}; // ✅ Full row buffer (128 columns)

    for (int page = 0; page < 8; page++) { // ✅ 8 pages for 64 pixels
        writeCommand(0xB0 + page); // ✅ Set page address
        writeCommand(0x00); // ✅ Lower column start
        writeCommand(0x10); // ✅ Upper column start

        writeData(clearBuffer, 128); // ✅ Fast full-row clear
    }
}





// Declare function prototypes
void eraseBar(SSD1306 &display, int column_start, int width, int height);
void drawBar(SSD1306 &display, int column_start, int width, int height);
void drawPerformanceBars(SSD1306 &display);

// ✅ Define eraseBar() first
void eraseBar(SSD1306 &display, int column_start, int width, int height) {
    if (height <= 0) return;

    int full_pages = height / 8;
    int remainder = height % 8;

    for (int x = 0; x < width; x++) {
        for (int page = 7; page >= (8 - full_pages); page--) {
            display.writeCommand(0xB0 + page);
            display.writeCommand(0x00 + ((column_start + x) & 0x0F));
            display.writeCommand(0x10 + ((column_start + x) >> 4));
            uint8_t data = 0x00;
            display.writeData(&data, 1);
        }

        if (remainder > 0) {
            int top_page = 8 - full_pages - 1;
            display.writeCommand(0xB0 + top_page);
            display.writeCommand(0x00 + ((column_start + x) & 0x0F));
            display.writeCommand(0x10 + ((column_start + x) >> 4));
            uint8_t data = 0x00;
            display.writeData(&data, 1);
        }
    }
}

// ✅ Define drawBar() second
void drawBar(SSD1306 &display, int column_start, int width, int height) {
    int full_pages = height / 8;
    int remainder = height % 8;

    //if (height < 8) {
    //    remainder = 8;
    //    full_pages = 0;
    //}

    for (int x = 0; x < width; x++) {
        for (int page = 7; page >= (8 - full_pages); page--) {
            display.writeCommand(0xB0 + page);
            display.writeCommand(0x00 + ((column_start + x) & 0x0F));
            display.writeCommand(0x10 + ((column_start + x) >> 4));
            uint8_t data = 0xFF;
            display.writeData(&data, 1);
        }

        if (remainder > 0) {
            int top_page = 8 - full_pages - 1;
            display.writeCommand(0xB0 + top_page);
            display.writeCommand(0x00 + ((column_start + x) & 0x0F));
            display.writeCommand(0x10 + ((column_start + x) >> 4));
            uint8_t data = (1 << remainder) - 1;
            display.writeData(&data, 1);
        }
    }
}

// ✅ Now drawPerformanceBars() can call eraseBar() and drawBar()
void drawPerformanceBars(SSD1306 &display) {
    static int prev_gpu_usage = -1;
    static std::vector<int> prev_cpu_usage(8, -1);

    int gpu_usage = getGPUUsage();
    std::vector<int> cpu_usage = getCPUUsage();
    int num_cpu_cores = std::min((int)cpu_usage.size(), 8);

    int gpu_column_start = 10;
    int gpu_width = 10;

    int cpu_column_start = 50;
    int cpu_width = 5;
    int cpu_spacing = 3;

    if (gpu_usage != prev_gpu_usage) {
        int height_gpu = mapToHeight(gpu_usage);
        int prev_height_gpu = mapToHeight(prev_gpu_usage);
        eraseBar(display, gpu_column_start, gpu_width, prev_height_gpu);
        drawBar(display, gpu_column_start, gpu_width, height_gpu);
        prev_gpu_usage = gpu_usage;
    }

    for (int i = 0; i < num_cpu_cores; i++) {
        if (cpu_usage[i] != prev_cpu_usage[i]) {
            int height_cpu = mapToHeight(cpu_usage[i]);
            int prev_height_cpu = mapToHeight(prev_cpu_usage[i]);
            int bar_x_start = cpu_column_start + (i * (cpu_width + cpu_spacing));
            eraseBar(display, bar_x_start, cpu_width, prev_height_cpu);
            drawBar(display, bar_x_start, cpu_width, height_cpu);
            prev_cpu_usage[i] = cpu_usage[i];
        }
    }
}


// ✅ Now, main() can call resetI2CBus()
int main() {

    resetI2CBus();  // **Reset before using OLED**
    SSD1306 display;
    display.clearDisplayFast();
    while (true) {
        drawPerformanceBars(display);
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Refresh every second
    }

    return 0;

}
