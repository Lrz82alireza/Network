# تنظیمات کامپایلر
CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall

# مسیرها و فایل‌های مربوط به سرور
SERVER_DIR = Server
SERVER_SRC = $(SERVER_DIR)/server.cpp $(SERVER_DIR)/client_handler.cpp
SERVER_OUT = $(SERVER_DIR)/server

# مسیرها و فایل‌های مربوط به کلاینت
CLIENT_DIR = Client
CLIENT_SRC = $(CLIENT_DIR)/client.cpp
CLIENT_OUT = $(CLIENT_DIR)/client

# هدف پیش‌فرض که همه چیز را کامپایل می‌کند
all: $(SERVER_OUT) $(CLIENT_OUT)

# کامپایل سرور
$(SERVER_OUT): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o $(SERVER_OUT)

# کامپایل کلاینت
$(CLIENT_OUT): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(CLIENT_SRC) -o $(CLIENT_OUT)

# هدف run برای اجرای پروژه در tmux
run: all
	# اگر سشنی به اسم network_project وجود داشت، آن را ببند
	@if tmux ls 2>/dev/null | grep -q "network_project"; then \
		tmux kill-session -t network_project; \
	fi
	
	# ایجاد یک سشن جدید در بک‌گراند
	@tmux new-session -d -s network_project

	# تقسیم صفحه به دو قسمت چپ و راست (چپ برای سرور، راست برای کلاینت‌ها)
	@tmux split-window -h -t network_project:0

	# اجرای سرور در پنل سمت چپ (pane 0)
	@tmux send-keys -t network_project:0.0 'cd $(SERVER_DIR) && ./server || echo "Server failed"' C-m
	
	# صبر کوتاه برای بالا آمدن کامل سرور
	@sleep 1

	# تقسیم پنل سمت راست (pane 1) به بالا و پایین (کلاینت ۱ و ۲)
	@tmux split-window -v -t network_project:0.1
	
	# تقسیم پنل پایینِ سمت راست (pane 2) مجددا به بالا و پایین (کلاینت ۲ و ۳)
	@tmux split-window -v -t network_project:0.2

	# اجرای کلاینت‌ها در پنل‌های سمت راست
	@tmux send-keys -t network_project:0.1 'cd $(CLIENT_DIR) && ./client' C-m
	@tmux send-keys -t network_project:0.2 'cd $(CLIENT_DIR) && ./client' C-m
	@tmux send-keys -t network_project:0.3 'cd $(CLIENT_DIR) && ./client' C-m

	# اتصال به محیط tmux
	@tmux attach-session -t network_project

# هدف clear برای پاک کردن فایل‌های کامپایل شده
clear:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)
	@echo "Cleaned executable files."
