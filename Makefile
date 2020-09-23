INC = -I ./include
OBJ = obj
SBIN = sbin
SRC_C = $(wildcard src/*.c)
SRC_C += $(wildcard tools/*c)
OBJ_O = $(addprefix $(OBJ)/, $(patsubst %.c,%.o,$(notdir $(SRC_C))))

TATGET = $(addprefix sbin/, PreSys)
$(info SRC_C  is ${SRC_C})
$(info TATGET is ${TATGET})
$(info OBJ_O  is ${OBJ_O})

$(OBJ)/%.o: tools/%.c
	gcc  -g -c -Wall ${INC}  $< -o $@

$(OBJ)/%.o: src/%.c
	gcc  -g -c -Wall ${INC}  $< -o $@

$(TATGET): $(OBJ_O)
	gcc -g -pthread -o $@ $^

.PHONY: clean

clean:
	rm -f $(OBJ)/* $(TATGET)
