NAME = webserv
SRC = src/Server.cpp src/Buffer.cpp src/ClientConnection.cpp src/main.cpp src/ConfigParser.cpp
OBJ = $(SRC:.cpp=.o)
CC = c++
CFLAGS = -Wall -Wextra -Werror -std=c++98 -g3
RM = rm -rf
INC = -Iinc/

%.o: %.cpp
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(NAME): $(OBJ)
	@$(CC) $(OBJ) -o $(NAME)

all: $(NAME)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
