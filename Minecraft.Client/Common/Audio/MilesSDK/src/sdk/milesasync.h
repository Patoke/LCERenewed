#pragma once


//
// need to have a callback for the current stream level
// need to schedule - 
//      switch to empties to get sound info.
//      switch to low streams to prevent starvation.
//      stay on existing if nothing urgent.
//      treat large sounds as streams? want to keep them active if possible, since they may be playing
//          already
//
//