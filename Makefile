.PHONY: all build build-daemon build-gui clean install user-install

BUILD_DIR := build
DAEMON_TARGET := daemon/target/release/runaway-daemon
GUI_TARGET := $(BUILD_DIR)/runaway-gui

all: build

build: build-daemon build-gui

build-daemon:
	cd daemon && cargo-1.89 build --release

build-gui:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ../gui -DCMAKE_BUILD_TYPE=Release && make -j$$(nproc)

clean:
	rm -rf $(BUILD_DIR)
	cd daemon && cargo-1.89 clean

install: build
	install -Dm755 $(DAEMON_TARGET) /usr/local/bin/runaway-daemon
	install -Dm755 $(GUI_TARGET) /usr/local/bin/runaway-gui
	install -Dm644 config/default.toml /usr/local/share/runaway-guard/default.toml

user-install:
	mkdir -p ~/.config/systemd/user
	cp scripts/runaway-guard.service ~/.config/systemd/user/
	systemctl --user daemon-reload
	systemctl --user enable runaway-guard.service
