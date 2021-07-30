// Game of Othello -- Example of main
// Universidad Simon Bolivar, 2012.
// Author: Blai Bonet
// Last Revision: 1/11/16
// Modified by:

#include <iostream>
#include <limits>
#include "othello_cut.h" // won't work correctly until .h is fixed!
#include "utils.h"

#include <unordered_map>
#include <list>
#include <queue>
#include <set>

using namespace std;

unsigned long long expanded = 0;
unsigned long long generated = 0;
int tt_threshold = 32; // threshold to save entries in TT
const int INF = 200;

// Transposition table (it is not necessary to implement TT)
enum { EXACT, LOWER, UPPER };
struct stored_info_t {
    int value_;
    int type_;
    stored_info_t(int value = -100, int type = LOWER) : value_(value), type_(type) { }
};

struct hash_function_t {
    size_t operator()(const state_t &state) const {
        return state.hash();
    }
};

class hash_table_t : public unordered_map<state_t, stored_info_t, hash_function_t> {
};

hash_table_t TTable[2];

//int maxmin(state_t state, int depth, bool use_tt);
//int minmax(state_t state, int depth, bool use_tt = false);
//int maxmin(state_t state, int depth, bool use_tt = false);
int negamax(state_t state, int color);
int negamax(state_t state, int alpha, int beta, int color, bool use_tt = false);
int scout(state_t state, int color);
int negascout(state_t state, int alpha, int beta, int color);
int sss_star(state_t state, int color, int bound);
int mtdf(state_t root, int color, int f);

int main(int argc, const char **argv) {
    state_t pv[128];
    int npv = 0;
    for ( int i = 0; PV[i] != -1; ++i ) ++npv;

    int algorithm = 0;
    if ( argc > 1 ) algorithm = atoi(argv[1]);
    bool use_tt = argc > 2;

    // Extract principal variation of the game
    state_t state;
    cout << "Extracting principal variation (PV) with " << npv << " plays ... " << flush;
    for ( int i = 0; PV[i] != -1; ++i ) {
        bool player = i % 2 == 0; // black moves first!
        int pos = PV[i];
        pv[npv - i] = state;
        state = state.move(player, pos);
    }
    pv[0] = state;
    cout << "done!" << endl;

#if 0
    // print principal variation
    for ( int i = 0; i <= npv; ++i )
        cout << pv[npv - i];
#endif

    // for MTD(f)
    int f;

    // Print name of algorithm
    cout << "Algorithm: ";
    if ( algorithm == 1 )
        cout << "Negamax (minmax version)";
    else if ( algorithm == 2 )
        cout << "Negamax (alpha-beta version)";
    else if ( algorithm == 3 )
        cout << "Scout";
    else if ( algorithm == 4 )
        cout << "Negascout";
    else if ( algorithm == 5 )
        cout << "SSS*";
    else if ( algorithm == 6 ) {
        f = atoi(argv[2]);
        cout << "MTD(f) with " << f;
    }
    cout << (use_tt ? " w/ transposition table" : "") << endl;

    // Run algorithm along PV (bacwards)
    cout << "Moving along PV:" << endl;
    for ( int i = 0; i <= npv; ++i ) {
        //cout << pv[i];
        int value = 0;
        TTable[0].clear();
        TTable[1].clear();
        float start_time = Utils::read_time_in_seconds();
        expanded = 0;
        generated = 0;
        int color = i % 2 == 1 ? 1 : -1;

        try {
            if ( algorithm == 1 ) {
                value = color * negamax(pv[i], color);
            } else if ( algorithm == 2 ) {
                value = color * negamax(pv[i], -INF, INF, color);
            } else if ( algorithm == 3 ) {
                value = scout(pv[i], color);
            } else if ( algorithm == 4 ) {
                value = color * negascout(pv[i], -INF, INF, color);
            } else if (algorithm == 5 ) {
                value = sss_star(pv[i], color, INF);
            } else if (algorithm == 6) {
                value = color * mtdf(pv[i], color, f);
            }
        } catch ( const bad_alloc &e ) {
            cout << "size TT[0]: size=" << TTable[0].size() << ", #buckets=" << TTable[0].bucket_count() << endl;
            cout << "size TT[1]: size=" << TTable[1].size() << ", #buckets=" << TTable[1].bucket_count() << endl;
            use_tt = false;
        }

        float elapsed_time = Utils::read_time_in_seconds() - start_time;

        cout << npv + 1 - i << ". " << (color == 1 ? "Black" : "White") << " moves: "
             << "value=" <<  value
             << ", #expanded=" << expanded
             << ", #generated=" << generated
             << ", seconds=" << elapsed_time
             << ", #generated/second=" << generated / elapsed_time
             << endl;
    }

    return 0;
}


int negamax(state_t state, int color) {
    ++generated;
    if (state.terminal())
        return color * state.value();

    int score = -INF;
    // booleano para saber si logre moverme colocando alguna ficha
    // o si tengo que pasar el turno al otro
    bool moved = false;
    for (int p : state.get_moves(color == 1)) {
        moved = true;
        score = max(
                    score,
                    -negamax(state.move(color == 1, p), -color)
                );
    }
    // si no logre moverme sigo en el mismo estado pero cambio el color
    if (!moved)
        score = -negamax(state, -color);

    ++expanded;
    return score;
}

int negamax(state_t state, int alpha, int beta, int color, bool use_tt) {
    ++generated;
    int original_alpha = alpha;

    if (use_tt && TTable[color == 1].find(state) != TTable[color == 1].end()) {
        auto tup = TTable[color == 1][state];

        if (tup.type_ == EXACT) {
            return tup.value_;
        }
        else if (tup.type_ == UPPER) {
            beta = min(beta, tup.value_);
        }
        else if (tup.type_ == LOWER) {
            alpha = max(alpha, tup.value_);
        }

        if (alpha >= beta)
            return tup.value_;
    }

    if (state.terminal())
        return color * state.value();

    int score = -INF;
    // booleano para saber si logre moverme colocando alguna ficha
    // o si tengo que pasar el turno al otro
    bool moved = false;
    for (int p : state.get_moves(color == 1)) {
        moved = true;
        score = max(
                    score,
                    -negamax(state.move(color == 1, p), -beta, -alpha, -color, use_tt)
                );
        alpha = max(alpha, score);
        if (alpha >= beta)
            break;
    }
    // si no logre moverme sigo en el mismo estado pero cambio el color
    if (!moved)
        score = -negamax(state, -beta, -alpha, -color, use_tt);

    if (use_tt) {
        stored_info_t tup = {score, EXACT};
        if (score <= original_alpha)
            tup.type_ = UPPER;
        else if (score >= beta)
            tup.type_ = LOWER;
        TTable[color == 1][state] = tup;
    }

    ++expanded;
    return score;
}

// cond : 0 es > ; 1 es >=
bool test(state_t state, int color, int score, bool cond) {

    ++generated;
    if (state.terminal())
        return (cond ? state.value() >= score : state.value() > score);

    ++expanded;
    auto moves = state.get_moves(color == 1);
    for (int i = 0; i < (int)moves.size(); ++i) {
        auto child = state.move(color == 1, moves[i]);
        if (color == 1 && test(child, -color, score, cond))
            return true;
        if (color == -1 && !test(child, -color, score, cond))
            return false;
    }

    if (moves.size() == 0) {
        if (color == 1 && test(state, -color, score, cond))
            return true;
        if (color == -1 && !test(state, -color, score, cond))
            return false;
    }

    return color == -1;
}

int scout(state_t state, int color) {
    ++generated;
    if (state.terminal())
        return state.value();

    int score = 0;
    auto moves = state.get_moves(color == 1);
    for (int i = 0; i < (int)moves.size(); ++i) {
        auto child = state.move(color == 1, moves[i]);
        // primer hijo
        if (i == 0)
            score = scout(child, -color);
        else {
            if (color == 1 && test(child, -color, score, 0))
                score = scout(child, -color);
            if (color == -1 && !test(child, -color, score, 1))
                score = scout(child, -color);
        }
    }
    // no se logro poner fichas, pasar turno
    if (moves.size() == 0)
        score = scout(state, -color);

    ++expanded;
    return score;
}

int negascout(state_t state, int alpha, int beta, int color) {

    ++generated;
    if (state.terminal())
        return color * state.value();

    int score;
    auto moves = state.get_moves(color == 1);
    for (int i = 0; i < (int)moves.size(); ++i) {
        auto child = state.move(color == 1, moves[i]);
        // primer hijo
        if (i == 0)
            score = -negascout(child, -beta, -alpha, -color);
        else {

            score = -negascout(child, -alpha - 1, -alpha, -color);
            if (alpha < score && score < beta)
                score = -negascout(child, -beta, -score, -color);
        }

        alpha = max(alpha, score);
        if (alpha >= beta)
            break;
    }

    // no se logro poner fichas, pasar turno
    if (moves.size() == 0)
        alpha = -negascout(state, -beta, -alpha, -color);


    ++expanded;
    return alpha;
}






// SSS*
struct sss_state {
    state_t othello;
    int color;
    bool root;
    sss_state *father;
    list<int> moves;

    sss_state(state_t ot, sss_state *fat, int col) {
        othello = ot;
        father = fat;
        color = col;
        root = false;

        auto moves_aux = othello.get_moves(color == 1);
        for (int i = 0; i < (int)moves_aux.size(); ++i)
            moves.push_back(moves_aux[i]);
        if (moves.size() == 0)
            moves.push_back(INF);
    }
};


int sss_star(state_t n, int color, int boud) {
    // h value, state * and bool (true is live, false is dead)
    priority_queue<tuple<int, sss_state*, bool>> pq;

    sss_state *r = new sss_state(n, nullptr, color);
    r->root = true;
    pq.push({INF, r, true});

    int ret = -42;

    set<sss_state*> ignore_childs;
    while (true) {
        int h = get<0>(pq.top());
        sss_state *state = get<1>(pq.top());
        bool live = get<2>(pq.top());
        pq.pop();

        // esta es la parte de purgar hijos, la implemente asi: si tu padre purgo a sus hijos,
        // entonces purgas a los tuyos y no entras de nuevo en la siguiente parte del codigo
        if (state->father != nullptr && ignore_childs.find(state->father) != ignore_childs.end()) {
            ignore_childs.insert(state);
            continue;
        }

        if (live) {
            if (state->othello.terminal()) {
                pq.push({min(state->othello.value(), h), state, false});
            }
            else if (state->color == -1) {  //min
                state_t child_othello = state->othello.move(state->color == 1, state->moves.front());
                state->moves.pop_front();

                sss_state *child = new sss_state(child_othello, state, -state->color);
                pq.push({h, child, true});
            }
            else if (state->color == 1) {  //max
                for (auto move : state->moves) {
                    state_t child_othello = state->othello.move(state->color == 1, move);

                    sss_state *child = new sss_state(child_othello, state, -state->color);
                    pq.push({h, child, true});
                }
            }
        }
        else {
            if (state->root) {
                ret = h;
                break;
            }
            else if (state->color == -1) {  //min
                ignore_childs.insert(state->father);
                pq.push({h, state->father, false});
            }
            else if (state->color == 1) {  //max
                if (!state->father->moves.empty()) {
                    state_t brother_othello = state->father->othello.move(state->father->color == 1, state->father->moves.front());
                    state->father->moves.pop_front();

                    sss_state *brother = new sss_state(brother_othello, state->father, -state->father->color);
                    pq.push({h, brother, true});
                }
                else {
                    pq.push({h, state->father, false});
                }
            }
        }
    }

    return ret;
}



// mtd(f) the better the guess for the f value, the quicker the solution is found
int mtdf(state_t root, int color, int f) {
    int bound[2] = { -INF, INF};
    do {
        int beta = f + (f == bound[0]);
        f = negamax(root, beta - 1, beta, color, true);
        bound[f < beta] = f;
    } while (bound[0] < bound[1]);
    return f;
}
