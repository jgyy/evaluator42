NAME = eval
CXX  = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17

SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)

CPR_DIR     = cpr
CPR_BUILD   = $(CPR_DIR)/build
CPR_INCLUDE = $(CPR_BUILD)/include
CPR_LIB_DIR = $(CPR_BUILD)/lib

NLOHMANN_DIR      = nlohmann
NLOHMANN_INCLUDE  = $(NLOHMANN_DIR)/single_include

LIBS = -L$(CPR_LIB_DIR) -lcpr -lcurl -lssl -lcrypto -lz -ldl -lpthread

.PHONY: all clean fclean re submodule-cpr cpr submodule-nlohmann nlohmann

all: $(NAME)

submodule-cpr:
	git submodule update --init --recursive $(CPR_DIR)

submodule-nlohmann:
	git submodule update --init --recursive $(NLOHMANN_DIR)

submodules: submodule-cpr submodule-nlohmann

$(CPR_LIB_DIR)/libcpr.a: submodule-cpr
	mkdir -p $(CPR_BUILD)
	cd $(CPR_BUILD) && \
	cmake .. \
	    -DBUILD_SHARED_LIBS=OFF \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	    -DCMAKE_BUILD_TYPE=Release \
	    -DBUILD_CPR_TESTS=OFF \
	    -DCMAKE_INSTALL_PREFIX=$(PWD)/$(CPR_BUILD) && \
	$(MAKE) -j $(shell nproc) && \
	$(MAKE) install

.PHONY: cpr
cpr: $(CPR_LIB_DIR)/libcpr.a

.PHONY: nlohmann
nlohmann: submodule-nlohmann

%.o: %.cpp cpr nlohmann
	$(CXX) $(CXXFLAGS) \
	    -I$(CPR_INCLUDE) \
	    -I$(NLOHMANN_INCLUDE) \
	    -c $< -o $@

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME) $(LIBS)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)
	rm -rf $(CPR_BUILD)

re: fclean all
