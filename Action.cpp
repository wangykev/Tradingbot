#include "Action.h"

bool repeatPatterns(Actions &actions)
{
	if (actions.size() > 20) {
		while (actions.size() > 50) actions.pop_back();

		if (actions[0] == actions[1] && actions[1] == actions[2] && actions[2] == actions[3]) {
			return true;  //find repeating pattern
		}

		if (actions[0] == actions[2] && actions[1] == actions[3] && actions[2] == actions[4] && actions[3] == actions[5] && actions[4] == actions[6]) {
			return true;  //find repeating pattern
		}

		if (actions[0] == actions[3] && actions[1] == actions[4] && actions[2] == actions[5] && actions[3] == actions[6] && actions[4] == actions[7] && actions[5] == actions[8]) {
			return true;  //find repeating pattern
		}
		if (actions[0] == actions[4] && actions[1] == actions[5] && actions[2] == actions[6] && actions[3] == actions[7] && actions[4] == actions[8] && actions[5] == actions[9]) {
			return true;  //find repeating pattern
		}
	}
	return false;
}