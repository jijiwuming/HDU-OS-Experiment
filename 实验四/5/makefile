obj=receive sender
target: $(obj)
$(obj):
	cc -pthread -o $@ $@.c common.c
.PHONY : clean
clean:$(obj)
	rm $(obj)
