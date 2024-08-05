# use the startup function to hide extra variables that 
# we don't really want users to mess with without
# understanding what they do

import engine
import slcan
canbus = engine.can()
output = slcan.slcan()

del slcan # hide this from the scope so people don't accidentally mess with it

engine.start_engine(canbus,output, globals())