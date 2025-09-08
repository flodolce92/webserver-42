# =============================================================================
# Colors for Output
# =============================================================================
GREEN		=	\033[0;32m
YELLOW		=	\033[0;33m
RED			=	\033[0;31m
BLUE		=	\033[0;34m
RESET		=	\033[0m

# =============================================================================
# Directories and Files
# =============================================================================
SRCS_DIR	=	src/
OBJS_DIR	=	obj/
INC_DIR		=	inc/

SRC			=	Server.cpp \
				Buffer.cpp \
				ClientConnection.cpp \
				main.cpp \
				ConfigParser.cpp \
				ConfigManager.cpp \
				Request.cpp \
				Response.cpp \
				CGIHandler.cpp \
				FileServer.cpp \
				StatusCodes.cpp
SRCS		=	$(addprefix $(SRCS_DIR), $(SRC))
OBJS		=	$(addprefix $(OBJS_DIR), $(SRC:.cpp=.o))

# =============================================================================
# Compiler and Flags
# =============================================================================
NAME		=	webserv
CXX			=	g++
CXXFLAGS	=	-Wall -Wextra -Werror -g3 -std=c++98
INC			=	-I $(INC_DIR)
RM			=	rm -f

# =============================================================================
# Rules
# =============================================================================

# Default rule: Build the executable
all:			$(NAME)

# Create the object directory
$(OBJS_DIR):
				@echo "$(YELLOW)Creating object directory...$(RESET)"
				@mkdir -p $(OBJS_DIR)

# Compile source files into object files
$(OBJS_DIR)%.o:	$(SRCS_DIR)%.cpp | $(OBJS_DIR) $(INC_DIR)
				@echo "$(BLUE)Compiling $<...$(RESET)"
				@$(CXX) $(CXXFLAGS) -c $< -o $@ $(INC)

# Link object files to create the executable
$(NAME):		$(OBJS_DIR) $(OBJS)
				@echo "$(GREEN)Linking objects and creating $(NAME)...$(RESET)"
				@$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)
				@echo "$(GREEN)Creating upload directory...$(RESET)"
				@mkdir -p ./www/html/uploads
				@echo "$(GREEN)Build complete!$(RESET)"

# Clean object files
clean:
				@echo "$(RED)Cleaning object files...$(RESET)"
				@$(RM) $(OBJS)
				@$(RM) -r $(OBJS_DIR)

# Clean object files and remove the executable
fclean:			clean
				@echo "$(RED)Removing executable...$(RESET)"
				@$(RM) $(NAME)

# Rebuild the project from scratch
re:				fclean all

# Debug build: Compile with debug flags
debug:
				@$(MAKE) CXXFLAGS="$(CXXFLAGS) -DDEBUG" --no-print-directory re

# =============================================================================
# Phony Targets
# =============================================================================
.PHONY:			all clean fclean re debug
