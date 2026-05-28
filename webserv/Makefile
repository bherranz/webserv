# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: jaime <jaime@student.42.fr>                +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/03/20 15:49:56 by miparis           #+#    #+#              #
#    Updated: 2026/05/28 13:05:00 by jaime            ###   ########.fr        #
#                                                                              #
# **************************************************************************** #


NAME = Webserv

SRCS =  src/main.cpp \
		src/server/Server.cpp \
		src/server/Client.cpp \
		src/config/Config.cpp \
		src/config/ConfigParser.cpp

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