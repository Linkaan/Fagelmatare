MODULES := $(wildcard */.)

.PHONY: all $(MODULES)

all: $(MODULES)

$(MODULES):
	$(MAKE) -C $@
