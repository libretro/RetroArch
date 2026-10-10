# Objects of sources outside the sample go under its own obj/, so no
# sample links another's leftovers built with other flags. ../ is kept
# as __/ there; sample_src maps it back:
#   OBJS := $(call sample_objs,$(SOURCES:.c=.o))
#   %.o: $$(call sample_src,$$*).c

SAMPLE_OBJDIR ?= obj

sample_objs = $(foreach o,$(1),$(if $(filter ../% /%,$(o)),$(SAMPLE_OBJDIR)/$(subst ../,__/,$(o)),$(o)))
sample_src  = $(subst __/,../,$(patsubst $(SAMPLE_OBJDIR)/%,%,$(1)))

.SECONDEXPANSION:
