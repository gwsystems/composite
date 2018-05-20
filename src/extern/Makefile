.PHONY: build, init, clean

default: build

init:
	$(info )
	$(info ***********************************************)
	$(info ********** Setting up External Libs ***********)
	$(info ***********************************************)
	$(info )
	bash -c 'cd cFE/ && source setvars.sh && make -C build/cpu1 config'
	$(shell cd cFE/ && python make.py -p >&2)


build:
	$(info )
	$(info ***********************************************)
	$(info *********** Building External Libs ************)
	$(info ***********************************************)
	$(info )
	$(shell cd cFE && python make.py >&2)

clean:
	$(info )
	$(info ***********************************************)
	$(info *********** Cleaning External Libs ************)
	$(info ***********************************************)
	$(info )
	$(shell cd cFE && python make.py -c >&2)
