#!/bin/bash
#mv grammar_begin.inl grammar_begin.h && make indent && mv grammar_begin.h grammar_begin.inl
for i in *.inl ; do
  mv $i ${i/.inl/_inl.h}
done
make indent
for i in *_inl.h ; do
  mv $i ${i/_inl.h/.inl}
done
