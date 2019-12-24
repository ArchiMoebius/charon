# Thx https://tech.davis-hansson.com/p/make/
SHELL := bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules
CFLAGS=-fPIC -Wall -I./includes
CC := gcc

ifeq ($(origin .RECIPEPREFIX), undefined)
  $(error This Make does not support .RECIPEPREFIX. Please use GNU Make 4.0 or later)
endif
.RECIPEPREFIX = >

all: clean libcharon.a

libcharon.a: charon.c
> mkdir build
> $(CC) -o build/charon.o -c $< $(CFLAGS)
> ar rcs build/libcharon.a build/charon.o

clean:
> rm -rf build
.PHONY: clean
