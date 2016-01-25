all: ex01.tex
	pdflatex $^
	pdflatex $^

.PHONY: clean
clean:
	rm -f *.log *.toc *.nav *.aux *.snm *.vrb *.out

.PHONY: cleanall
cleanall: clean
	rm -f *.pdf


