
REMOTE_PORT := 4466
LOCAL_PORT := 52698
SSH_CONFIG_PATH := $(HOME)/.ssh/config
INCLUDE_LINE := Include  ~\/.remotelink\/ssh\/config

ifeq ($(shell uname -s), Darwin)
	SED_COMMAND = sed -i '' '1,/^$$/ s/^$$/$(INCLUDE_LINE)\n/g'
else
	SED_COMMAND = sed -i '1,/^$$/ s/^$$/$(INCLUDE_LINE)\n/g' 
endif

define ssh_config
Match Exec "! ps -p $$(ps -p $$$$ -o ppid=)| grep -q 'sftp'"
	LocalCommand $(HOME)/.remotelink/scripts/local_handler.sh %n &
	PermitLocalCommand yes
	RemoteForward $(REMOTE_PORT) localhost:$(LOCAL_PORT)
	RemoteCommand bash -c '$(remote_handler); zsh -l;'
	RequestTTY yes
endef

define local_handler
LOCAL_DIR="$(HOME)/.remotelink/files"
REMOTE_DIR="/root/.remotelink/files"

if ! lsof -i :52698 >/dev/null 2>&1; then
	$(HOME)/.remotelink/bin/macd >/dev/null 2>&1 &
fi

if ! pgrep -f "sshfs $$1" >/dev/null; then
	while true; do
		sshfs $$1:$$REMOTE_DIR $$LOCAL_DIR -o follow_symlinks,ServerAliveInterval=15 >/dev/null 2>&1 &

		if mount | grep -q "$$LOCAL_DIR"; then
		    break
		else
		   	sleep 2
		fi
	done
fi
endef

define remote_handler
mkdir -p $$HOME/.remotelink/{bin,files} && if ! test -f $$HOME/.remotelink/bin/macctl; then echo "install_macctl" | curl telnet://localhost:4466 --max-time 2 2>/dev/null | gcc -x c -o $$HOME/.remotelink/bin/macctl - ; fi && if ! grep -q "^export PATH=\$$PATH:\$$HOME/.remotelink/bin" "$$HOME/.profile"; then echo "export PATH=\$$PATH:\$$HOME/.remotelink/bin" >> $$HOME/.profile; fi&&source $$HOME/.profile
endef

export ssh_config
export local_handler
export remote_handler

all: macdserver macctl
	@echo "***************************************"
	@echo "Application has been successfully built"
	@echo "***************************************"

macdserver:
	cd macd && go build -o ../build/macd macd/cmd/macd && cd ..

macctl:
	gcc -o build/macctl macd/cmd/server/macctl.c 

clean:
	rm -f build/macd build/macctl

install:
	@echo Create a ~/.remotelink/bin directory ...
	@mkdir -p $(HOME)/.remotelink/{bin,ssh,scripts,files}
	@cp build/macd $(HOME)/.remotelink/bin/
	@cp build/macctl $(HOME)/.remotelink/bin/

	@echo Create remotelink configuration file ...
	@echo "$$ssh_config" | tee $(HOME)/.remotelink/ssh/config >/dev/null 2>&1

	@echo "$$local_handler" | tee $(HOME)/.remotelink/scripts/local_handler.sh >/dev/null 2>&1
	@echo "$$remote_handler" | tee $(HOME)/.remotelink/scripts/remote_handler.sh >/dev/null 2>&1
	@chmod +x $(HOME)/.remotelink/scripts/local_handler.sh $(HOME)/.remotelink/scripts/remote_handler.sh

	@echo Add to .ssh/config ...
	@if ! grep -q "^$(INCLUDE_LINE)" "$(SSH_CONFIG_PATH)"; then \
		$(SED_COMMAND) $(SSH_CONFIG_PATH); \
	fi
	
	@echo Add to .zprofile ...
	@if ! grep -q "^export PATH=\$$PATH:\$$HOME/.remotelink/bin" "$(HOME)/.zprofile"; then \
		@echo "export PATH=\$$PATH:\$$HOME/.remotelink/bin" >> $(HOME)/.zprofile; \
		@source $(HOME)/.zprofile; \
	fi
	
.PHONY: all clean install

