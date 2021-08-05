MOD=$(shell find . -maxdepth 1 | grep 'mod_')
CLEAN=$(subst mod_,clean_,${MOD})
.PHONY:${MOD}
all:${MOD}
${MOD}:
	@make -C $@

clean:${CLEAN}
${CLEAN}:
	@make -C $(subst clean_,mod_,$@) clean
