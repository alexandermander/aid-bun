CMDS := $(notdir $(wildcard cmd/*))
BINS := $(addprefix bin/,$(CMDS))

all: build
build: $(BINS)

.PHONY: all build clean test $(CMDS)

$(BINS): | bin
	go build -buildmode=pie -trimpath -o $@ ./cmd/$(@F)

bin:
	mkdir -p bin

$(CMDS): %: bin/%

test:
	go test ./...

clean:
	rm -f $(BINS)
