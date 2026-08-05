# arbor-core — Agda mechanization
#
#   make check    typecheck everything under --safe
#   make status   report proved / postulated / by-construction against the paper
#   make labels   report arbor-core.tex labels with no counterpart in Meta.agda
#   make clean    drop interface files

AGDA ?= agda
SCRIPTS = ../scripts

.PHONY: check status labels clean

check:
	$(AGDA) --safe Everything.agda

status:
	@sh $(SCRIPTS)/status.sh

labels:
	@sh $(SCRIPTS)/check-labels.sh ../paper/arbor-core.tex Arbor

clean:
	rm -rf _build
	find . -name '*.agdai' -delete
