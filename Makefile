INC = -I ./include
OBJ = obj
SBIN = sbin
SRC_C = $(wildcard src/*.c)
SRC_C += $(wildcard tools/*c)
OBJ_O = $(addprefix $(OBJ)/, $(patsubst %.c,%.o,$(notdir $(SRC_C))))
MYSQL_LIB = -L /usr/lib64/mysql
MYSQL_SO = -l mysqlclient

TATGET = $(addprefix sbin/, PreSys)
$(info SRC_C  is ${SRC_C})
$(info TATGET is ${TATGET})
$(info OBJ_O  is ${OBJ_O})

$(OBJ)/%.o: tools/%.c
	gcc  -c -Wall ${INC}  $<  -o $@

$(OBJ)/%.o: src/%.c
	gcc  -c -Wall ${INC}  $<  -o $@

$(TATGET): $(OBJ_O)
	gcc -pthread ${MYSQL_LIB} ${MYSQL_SO} -o $@ $^

.PHONY: clean

clean:
	rm -f $(OBJ)/* $(TATGET)
