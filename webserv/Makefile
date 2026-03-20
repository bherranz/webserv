# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: miparis <miparis@student.42madrid.com>     +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/03/20 15:49:56 by miparis           #+#    #+#              #
#    Updated: 2026/03/20 15:53:47 by miparis          ###   ########.fr        #
#                                                                              #
# **************************************************************************** #


NAME = Webserv

SRCS =  CgiHandler.cpp \
		Config.cpp \
		ConfigParser.cpp \
		HttpRequest.cpp \
		HttpResponse.cpp \
		Router.cpp \
		Client.cpp \
		Server.cpp \
		Utils.cpp

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
	@$(CC) $(CFLAGS) $(CPPFLAGS) $(OBJS) -o $(NAME)
	@echo "$(BOLD)$(BLUE) 🖥️ Webserv operating $(NC)"

clean:
	@$(REMOVE) $(OBJS)

fclean: clean
	@$(REMOVE) $(NAME)
	@echo "$(BOLD)$(GREEN)🧹 Everything cleaned up!$(NC)"

re: fclean all

clear:
	@clear

.PHONY: all clean fclean re clear