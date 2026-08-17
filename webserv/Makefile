
NAME = webserv

SRCS =  src/main.cpp \
		src/server/Server.cpp \
		src/server/Client.cpp \
		src/config/Config.cpp \
		src/config/ParseFile.cpp \
		src/config/ParseBlock.cpp \
		src/config/ParseDirectives.cpp \
		src/config/ParseServerDirectives.cpp \
		src/config/ParseLocationsDirectives.cpp \
		src/config/configValidate.cpp \
		src/http/HttpRequest.cpp \
		src/http/HttpResponse.cpp \
		src/http/Router.cpp \
		src/cgi/CGIHandler.cpp \
		src/utils/Utils.cpp

# Define Colors
GREEN  := \033[1;32m  # Bold Green
YELLOW := \033[0;33m  # Non-bold Yellow
RED    := \033[0;31m  # Regular Red
BLUE   := \033[1;34m  # Bold Blue
PURPLE := \033[1;35m  # Bold Purple
BOLD   := \033[1m     # Bold Text
NC     := \033[0m     # No Color (Resets color and style)

OBJS = ${SRCS:.cpp=.o}


.SILENT: $(OBJS)

CC = c++
CFLAGS = -Wall -Wextra -Werror
CPPFLAGS = -std=c++98
REMOVE = rm -f

all: $(NAME)

$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) $(CPPFLAGS) -Iincludes $(OBJS) -o $(NAME)
	@echo "$(BOLD)$(BLUE) 🖥️ Webserv operating $(NC)"

%.o: %.cpp
	@$(CC) $(CFLAGS) $(CPPFLAGS) -Iincludes -c $< -o $@

clean:
	@$(REMOVE) $(OBJS)

fclean: clean
	@$(REMOVE) $(NAME)
	@echo "$(BOLD)$(GREEN)🧹 Everything cleaned up!$(NC)"

re: fclean all

clear:
	@clear

.PHONY: all clean fclean re clear