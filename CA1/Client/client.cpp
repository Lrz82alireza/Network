#include "client.hpp"

namespace fs = std::filesystem;

Client::Client(std::string ip, int port) : server_ip(ip), server_port(port), client_socket(-1) {
    server_addr.sin_family = AF_INET; // ipv4
    server_addr.sin_port = htons(server_port);

    if (!fs::exists(client_storage_path)) {
        fs::create_directory(client_storage_path);
    }
}

Client::~Client() {
    if (client_socket != -1) {
        close(client_socket);
    }
}

void Client::setupSocket() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Error: Could not create socket!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // تبدیل آیپی متنی به آیپی باینری در ساختار شبکه
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Error: Invalid address / Address not supported!" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Client::connectToServer() {
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error: Connection to server failed!" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "[+] Connected to the server successfully!" << std::endl;
}

std::vector<std::string> Client::splitCommand(const std::string& command) {
    std::istringstream iss(command);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void Client::handlePutCommand(const std::string& filename) {
    std::string filepath = client_storage_path + filename;
    
    if (!fs::exists(filepath)) {
        std::cout << "[Local Error]: File '" << filename << "' not found in " << client_storage_path << ".\n";
        return;
    }
    
    long filesize = fs::file_size(filepath);
    std::string msg_to_send = "PUT " + filename + " " + std::to_string(filesize) + "\n";
    send(client_socket, msg_to_send.c_str(), msg_to_send.length(), 0);

    // منتظر ماندن برای دریافت سیگنال READY از Thread گیرنده
    while (!is_uploading) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ارسال بایت‌های فایل
    std::ifstream infile(filepath, std::ios::binary);
    char buffer[1024];
    while (infile.read(buffer, sizeof(buffer)) || infile.gcount() > 0) {
        send(client_socket, buffer, infile.gcount(), 0);
    }
    infile.close();
    
    is_uploading = false; // پایان آپلود
    std::cout << "[System]: File uploaded.\n";
}

void Client::handleGetCommand(const std::string& filename) {
    current_filename = filename; // ذخیره نام فایل برای Thread گیرنده
    std::string msg_to_send = "GET " + filename + "\n";
    send(client_socket, msg_to_send.c_str(), msg_to_send.length(), 0);
    // ادامه روند (دانلود فایل) توسط receiveMessages مدیریت می‌شود
}

void Client::receiveMessages() {
    char buffer[1024];
    while (true) {
        // اگر در وضعیت دانلود هستیم، داده‌های باینری فایل را دریافت می‌کنیم
        if (is_downloading) {
            std::ofstream outfile(client_storage_path + current_filename, std::ios::binary);
            long total_received = 0;
            
            while (total_received < expected_filesize) {
                int bytes_to_read = std::min((long)1024, expected_filesize - total_received);
                int bytes_received = recv(client_socket, buffer, bytes_to_read, 0);
                if (bytes_received <= 0) break;
                
                outfile.write(buffer, bytes_received);
                total_received += bytes_received;
            }
            outfile.close();
            
            std::cout << "\n[System]: File '" << current_filename << "' downloaded successfully.\n> " << std::flush;
            is_downloading = false;
            continue;
        }

        // دریافت پیام‌های عادی / کنترلی
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            std::cout << "\n[!] Disconnected from server." << std::endl;
            exit(0);
        }
        
        std::string msg(buffer);

        // بررسی هندشیک GET
        if (msg.find("FILE_SIZE ") == 0) {
            std::istringstream iss(msg);
            std::string token;
            long temp_filesize; // متغیر موقت
            
            iss >> token; // رد کردن کلمه FILE_SIZE
            iss >> temp_filesize; // خواندن مقدار در متغیر موقت
            
            expected_filesize = temp_filesize; // تخصیص به متغیر atomic
            
            is_downloading = true;
            
            // هندشیک: ارسال ACK به سرور
            std::string ack = "READY_FOR_GET\n";
            send(client_socket, ack.c_str(), ack.length(), 0);
        }

        // بررسی هندشیک PUT
        else if (msg.find("READY_FOR_FILE") == 0) {
            is_uploading = true; // باز کردن قفل برای handlePutCommand
        } 
        // چاپ پیام‌های معمولی
        else {
            std::cout << "\n" << msg << "\n> " << std::flush;
        }
    }
}

void Client::sendMessages() {
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        std::vector<std::string> tokens = splitCommand(input);
        if (tokens.empty()) continue;

        std::string command = tokens[0];

        if (command == "QUIT") {
            std::cout << "Disconnecting..." << std::endl;
            close(client_socket);
            exit(0);
        } 
        else if (command == "PUT" && tokens.size() >= 2) {
            handlePutCommand(tokens[1]);
        } 
        else if (command == "GET" && tokens.size() >= 2) {
            handleGetCommand(tokens[1]);
        } 
        else {
            std::string msg_to_send = input + "\n";
            send(client_socket, msg_to_send.c_str(), msg_to_send.length(), 0);
        }
    }
}

void Client::start() {
    setupSocket();
    connectToServer();

    std::thread receive_thread(&Client::receiveMessages, this);
    receive_thread.detach();

    sendMessages();
}


int main() {
    Client myClient("127.0.0.1", 8080);
    myClient.start();
    return 0;
}
