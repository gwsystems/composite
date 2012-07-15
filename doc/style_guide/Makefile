NAME=composite_coding_style
TEX_FILES=$(wildcard *.tex *.bib)

default: $(TEX_FILES)
	pdflatex $(NAME).tex

bib: default
	bibtex $(NAME)
	pdflatex $(NAME).tex
	pdflatex $(NAME).tex

show: default
	xpdf $(NAME).pdf

clean:
	rm *.aux *.out *.log
