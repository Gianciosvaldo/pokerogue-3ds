#!/usr/bin/env python3
"""Broad reliability/economy gates; no fixed win-rate promise."""
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    data = json.load(source)
assert data["seeds_per_squad"] == 512
assert data["difficulty_step_q8"] == 16 and data["exp_factor"] == 2000
assert len(data["squads"]) == 4
attempts = clears = matches = plays = 0
for squad in data["squads"]:
    reached, wins = squad["reached"], squad["wins"]
    assert len(reached) == len(wins) == 7
    assert reached[0] == squad["seasons"] == data["seeds_per_squad"]
    assert all(0 <= won <= seen for won, seen in zip(wins, reached))
    assert reached[1:] == wins[:-1]
    assert squad["clears"] == wins[-1]
    assert squad["matches"] == sum(reached)
    assert squad["matches"] <= squad["plays"] <= 512 * squad["matches"]
    assert 0 < squad["max_match_plays"] < 512
    earned = sum((500 + 100 * index) * won for index, won in enumerate(wins))
    assert squad["earned"] == earned == squad["spent"] + squad["remaining"]
    assert 0 <= squad["remaining"] <= 5600 * squad["seasons"]
    assert 0 <= squad["stones"] <= squad["affordable_rare_offers"] <= squad["rare_offers"] <= sum(wins)
    assert 0 <= squad["vitamins"] <= 2 * sum(wins)
    assert 125 * squad["vitamins"] + 1000 * squad["stones"] <= squad["spent"]
    assert squad["spent"] <= 275 * squad["vitamins"] + 1000 * squad["stones"]
    attempts += squad["seasons"]
    clears += squad["clears"]
    matches += squad["matches"]
    plays += squad["plays"]
assert attempts == 2048 and 0 < clears < attempts
print(f"football audit: {attempts} seeded seasons, {matches} real matches, {plays} snaps; termination, progression and economy passed")
