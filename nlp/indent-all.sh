#!/bin/bash
for i in *.inl ; do
  mv $i ${i/.inl/_inl.h}
done
make indent
for i in *_inl.h ; do
  mv $i ${i/_inl.h/.inl}
done
