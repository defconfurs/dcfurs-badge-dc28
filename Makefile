SRCDIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Each Directory contains a single animation.
SUBDIRS := $(shell find ${SRCDIR} -maxdepth 1 -type 'd' -printf "%f\n" | grep -v '^\.')
TARGETS := $(filter-out doc,$(SUBDIRS))

# The default rule.
all: $(TARGETS) animations.bin

# Create a unique target for each animation present
define create_target
$(eval \
$(1):
	@echo "Building [$$@]"
	@make -f $(abspath $(SRCDIR)/animation.mk) -C $(SRCDIR)/$(1)
)
endef
$(foreach T,$(TARGETS),$(call create_target,$(T)))

# Create a DFU image for the combined animation data.
animations.bin: $(TARGETS)
	@echo "Building [$@]"
	@truncate -s0 $@
	@$(foreach T,$(TARGETS),echo "   Adding     [$(T)]" && dd if=$(T)/$(T).bin bs=64K count=1 conv=sync >> $@ 2>/dev/null)

clean:
	rm -f animations.bin
	$(foreach T,$(TARGETS),@make -f $(abspath $(SRCDIR)/animation.mk) -C $T clean)

.PHONY: all animations clean $(TARGETS)
