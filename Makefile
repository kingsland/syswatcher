CORE_DIR=core
PLUGINS_DIR=plugins
TOOLS_DIR=tools

all:firmware plugin tool
firmware:
	make -C ./$(CORE_DIR)
plugin:
	make -C ./$(PLUGINS_DIR)
tool:
	make -C ./$(TOOLS_DIR)
clean:
	make -C ./$(CORE_DIR) clean
	make -C ./$(PLUGINS_DIR) clean
	make -C ./$(TOOLS_DIR) clean
