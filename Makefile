no-children.dylib: no-children.c
	$(CC) -dynamiclib -o $@ $< -flat_namespace -undefined dynamic_lookup
