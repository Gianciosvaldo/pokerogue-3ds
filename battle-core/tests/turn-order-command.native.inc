static const struct{uint32_t id;int species,form,level,hp,stats[6],stages[7];}command_actors[4]={
{11,242,0,1000,6260,{6260,355,355,1655,2855,1255},{0,0,0,0,0,0,0}},
{22,242,0,1000,6260,{6260,355,355,1655,2855,1255},{0,0,0,0,-1,0,0}},
{33,242,0,1000,6260,{6260,355,355,1655,2855,1255},{0,0,0,0,-3,0,0}},
{44,242,0,1000,6260,{6260,355,355,1655,2855,1255},{0,0,0,0,-2,0,0}},
};
static const pr_rng command_before[2]={{1,0.28535272739827633,0.9569424306973815,0.4407415797468275,0},{1,0.5061763995327055,0.7425215784460306,0.925082825589925,0}};
static const pr_rng command_after[2]={{1,0.28535272739827633,0.9569424306973815,0.4407415797468275,0},{652815,0.025558011839166284,0.8245645584538579,0.2163322693668306,0}};
static const uint32_t command_order[]={11,22,44,33,22,11,22,44,33,44,11,22,44,33,33,11,22,44,33};
static const struct{int hp,stages[7],pp[2],nh;pr_turn_move history[5];}command_expected[4]={
{2088,{0,0,0,0,0,0,0},{4,0},4,{{.move=552,.result=1,.use_mode=1,.n_targets=1,.targets={2}},{.move=552,.result=1,.use_mode=1,.n_targets=1,.targets={2}},{.move=552,.result=1,.use_mode=1,.n_targets=1,.targets={2}},{.move=552,.result=1,.use_mode=1,.n_targets=1,.targets={2}}}},
{6260,{0,0,2,0,-1,0,0},{0,1},5,{{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={2}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={2}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={2}},{.move=689,.result=1,.use_mode=1,.n_targets=1,.targets={0}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={2}}}},
{2992,{0,0,1,0,-3,0,0},{0,1},5,{{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}},{.move=689,.result=1,.use_mode=1,.n_targets=1,.targets={0}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}}}},
{6260,{0,0,2,0,-2,0,0},{0,1},5,{{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}},{.move=689,.result=1,.use_mode=1,.n_targets=1,.targets={0}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}},{.move=552,.result=1,.use_mode=3,.n_targets=1,.targets={0}}}},
};
static const int command_power[2]={1,1};
