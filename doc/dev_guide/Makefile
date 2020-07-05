RESOURCEDIR=./resources
CHAPTERS     =$(sort $(wildcard ??_*.md))

PANDOC=pandoc
# --highlight-style pygments another option vs listings
PANDOC_FLAGS=--toc --number-sections --listings --template=$(RESOURCEDIR)/eisvogel.tex --default-image-extension=pdf
VERSION=$(shell grep subtitle 00_title.md | awk -F 'Version ' '{print $$2}' | sed 's/"//')

PDFOUTPUT=composite_dev_manual_v$(VERSION).pdf

DEPGEN =python $(RESOURCEDIR)/sys_graph.py
NEATO  =neato
DOT    =dot
DEPTYPE=pdf
DEPOUT =$(RESOURCEDIR)/dependencies.$(DEPTYPE)
DEP2OUT=$(RESOURCEDIR)/dependencies_ranked.$(DEPTYPE)
INITGRF=$(RESOURCEDIR)/component_init.$(DEPTYPE)

TOCGEN =python $(RESOURCEDIR)/gen_toc.py

.PHONY: all images pdf web rmtmp
all: pdf web rmtmp

# use graphviz to generate the dependency graphs
images: $(DEPGENPROG)
	$(DEPGEN) | $(NEATO) -T$(DEPTYPE) > $(DEPOUT)
	$(DEPGEN) | $(DOT)   -T$(DEPTYPE) > $(DEP2OUT)
	$(DOT) -T$(DEPTYPE) $(RESOURCEDIR)/component_init.gf > $(INITGRF)

$(PDFOUTPUT): $(MANUAL_SRC) images
	$(PANDOC) $(CHAPTERS) `python resources/gen_blob_docs.py` -o $@ $(PANDOC_FLAGS)

# use pandoc to generate the book
pdf: images $(PDFOUTPUT) rmtmp

# We assume github rendering here, so creating the web docs just makes sure that the dependency images are ready
web: images pdf rmtmp
	$(TOCGEN) > composite_dev_manual_toc.md

rmtmp:
	rm -f /tmp/cos_docgen_*
