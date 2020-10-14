CORE_DIR=core
IPMISERVER_DIR=ipmiserver
PLUGINS_DIR=plugins
TOOLS_DIR=tools

all:firmware plugin tool #ipmisrv
firmware:
	make -C ./$(CORE_DIR)
plugin:
	make -C ./$(PLUGINS_DIR)
tool:
	make -C ./$(TOOLS_DIR)
ipmisrv:
	make -C ./$(IPMISERVER_DIR)
clean:
	make -C ./$(CORE_DIR) clean
	make -C ./$(PLUGINS_DIR) clean
	make -C ./$(TOOLS_DIR) clean
	make -C ./$(IPMISERVER_DIR) clean
