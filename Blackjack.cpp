#include "blackjack.h"
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace std;

struct SimulationResult {
    string strategy_name;
    int total_hands;
    int player_wins;
    int house_wins;
    int draws;
    double net_profit;
    double ev_per_hand;
    string log_file;
};

struct HandResult {
    addict hand;
    double bet;
    bool is_doubled;
    int initial_score;
};

int hilo_running_count = 0;
int ko_running_count = -20;

void update_counts(int card) {
    if (card >= 2 && card <= 6) hilo_running_count++;
    else if (card >= 10) hilo_running_count--;

    if (card >= 2 && card <= 7) ko_running_count++;
    else if (card >= 10) ko_running_count--;
}

int get_hilo_true_count(int cards_remaining) {
    double decks_remaining = std::max(1.0, cards_remaining / 52.0);
    return std::round(hilo_running_count / decks_remaining);
}

int draw_card_from_deck(vector<int>& deck) {
    if (deck.empty()) return 0;
    int card = deck.back();
    deck.pop_back();
    update_counts(card);
    return card;
}

vector<int> initialize_deck(int num_decks = 6) {
    vector<int> deck;
    vector<int> card_values = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10, 11 };
    for (int i = 0; i < num_decks; ++i) {
        for (int suit = 0; suit < 4; ++suit) {
            for (int val : card_values) { deck.push_back(val); }
        }
    }
    random_device rd;
    mt19937 g(rd());
    shuffle(deck.begin(), deck.end(), g);
    return deck;
}

void deal_initial_cards(HandResult& initial_hand, house& dealer, vector<int>& deck) {
    initial_hand.hand.draw(draw_card_from_deck(deck));
    dealer.draw(draw_card_from_deck(deck));
    initial_hand.hand.draw(draw_card_from_deck(deck));
    dealer.draw(draw_card_from_deck(deck));
}

void process_splits(vector<HandResult>& active_hands, double current_bet, vector<int>& deck, int dealer_upcard) {
    for (size_t j = 0; j < active_hands.size(); ++j) {
        while (active_hands[j].hand.split_cards(dealer_upcard) && active_hands.size() < 4) {
            int split_card = active_hands[j].hand.pop_last_card();
            HandResult new_hand = { addict(), current_bet, false, 0 };
            new_hand.hand.set_split();
            active_hands[j].hand.set_split();

            new_hand.hand.draw(split_card);
            active_hands[j].hand.draw(draw_card_from_deck(deck));
            new_hand.hand.draw(draw_card_from_deck(deck));

            active_hands[j].initial_score = active_hands[j].hand.getScore();
            new_hand.initial_score = new_hand.hand.getScore();

            active_hands.push_back(new_hand);
        }
    }
}

void play_dealer_hand(house& dealer, vector<HandResult>& active_hands, vector<int>& deck) {
    bool dealer_needs_to_play = false, all_player_blackjacks = true;
    for (auto& state : active_hands) {
        if (state.hand.getScore() <= 21 && !state.hand.is_blackjack()) { dealer_needs_to_play = true; all_player_blackjacks = false; break; }
        if (!state.hand.is_blackjack()) all_player_blackjacks = false;
    }

    if (dealer_needs_to_play || (!all_player_blackjacks && dealer.is_blackjack())) {
        while (dealer.should_draw() && dealer.getScore() < 21) dealer.draw(draw_card_from_deck(deck));
    }
}

// No counting sim

SimulationResult simulate_basic(int n) {
    double bankroll = 0.0;
    int base_unit = 10;
    int player_wins = 0, house_wins = 0, draws = 0, total_hands_played = 0;

    vector<int> deck = initialize_deck(6);
    ofstream outfile("log_basic.txt");
    if (!outfile.is_open()) { cerr << "Error: Could not open output file.\n"; exit(1); }
    outfile << "Hand\tP_Init\tD_Up\tP_Final\tD_Final\tWon\tCount\tProfit\n";

    for (int i = 0; i < n; ++i) {
        if (deck.size() < 78) deck = initialize_deck(6);

        vector<HandResult> active_hands;
        HandResult initial_hand = { addict(), (double)base_unit, false, 0 };
        house dealer;

        deal_initial_cards(initial_hand, dealer, deck);
        initial_hand.initial_score = initial_hand.hand.getScore();
        int dealer_upcard = dealer.get_upcard();
        active_hands.push_back(initial_hand);

        if (!dealer.is_blackjack() && !initial_hand.hand.is_blackjack()) {
            process_splits(active_hands, base_unit, deck, dealer_upcard);

            for (auto& state : active_hands) {
                if (state.hand.should_double(dealer_upcard, 0, Strategy::BASIC)) {
                    state.bet *= 2.0;
                    state.is_doubled = true;
                    state.hand.draw(draw_card_from_deck(deck));
                }
                else {
                    while (state.hand.should_draw(dealer_upcard, 0, Strategy::BASIC) && state.hand.getScore() < 21) {
                        state.hand.draw(draw_card_from_deck(deck));
                    }
                }
            }
            play_dealer_hand(dealer, active_hands, deck);
        }

        int d_score = dealer.getScore();
        bool d_bj = dealer.is_blackjack();

        for (auto& state : active_hands) {
            total_hands_played++;
            int p_score = state.hand.getScore();
            bool p_bj = state.hand.is_blackjack();
            double hand_profit = 0.0;

            if (p_score > 21) { hand_profit = -state.bet; house_wins++; }
            else if (p_bj && !d_bj) { hand_profit = state.bet * 1.5; player_wins++; }
            else if (p_bj && d_bj) { hand_profit = 0.0; draws++; }
            else if (d_bj) { hand_profit = -state.bet; house_wins++; }
            else if (d_score > 21) { hand_profit = state.bet; player_wins++; }
            else if (p_score > d_score) { hand_profit = state.bet; player_wins++; }
            else if (d_score > p_score) { hand_profit = -state.bet; house_wins++; }
            else { hand_profit = 0.0; draws++; }

            bankroll += hand_profit;
            int outcome = (hand_profit > 0) ? 2 : ((hand_profit == 0) ? 1 : 0);
            outfile << total_hands_played << "\t" << state.initial_score << "\t" << dealer_upcard << "\t"
                << p_score << "\t" << d_score << "\t" << outcome << "\t" << 0 << "\t" << hand_profit << "\n";
        }
    }
    outfile.close();
    return { "Basic Strategy Matrix (Flat Bet)", total_hands_played, player_wins, house_wins, draws, bankroll, bankroll / n, "log_basic.txt" };
}
//Hi-Lo counting sim
SimulationResult simulate_hilo(int n) {
    double bankroll = 0.0;
    int base_unit = 10;
    int player_wins = 0, house_wins = 0, draws = 0, total_hands_played = 0;

    vector<int> deck = initialize_deck(6);
    hilo_running_count = 0;
    ofstream outfile("log_hilo.txt");
    if (!outfile.is_open()) { cerr << "Error: Could not open output file.\n"; exit(1); }
    outfile << "Hand\tP_Init\tD_Up\tP_Final\tD_Final\tWon\tCount\tProfit\n";

    for (int i = 0; i < n; ++i) {
        if (deck.size() < 78) { deck = initialize_deck(6); hilo_running_count = 0; }

        int true_count = get_hilo_true_count(deck.size());
        double current_bet = base_unit;
        if (true_count >= 1) current_bet = base_unit * std::min(8, true_count + 1);

        vector<HandResult> active_hands;
        HandResult initial_hand = { addict(), current_bet, false, 0 };
        house dealer;

        deal_initial_cards(initial_hand, dealer, deck);
        initial_hand.initial_score = initial_hand.hand.getScore();
        int dealer_upcard = dealer.get_upcard();
        active_hands.push_back(initial_hand);

        if (!dealer.is_blackjack() && !initial_hand.hand.is_blackjack()) {
            process_splits(active_hands, current_bet, deck, dealer_upcard);

            for (auto& state : active_hands) {
                if (state.hand.should_double(dealer_upcard, true_count, Strategy::HILO)) {
                    state.bet *= 2.0;
                    state.is_doubled = true;
                    state.hand.draw(draw_card_from_deck(deck));
                }
                else {
                    while (state.hand.should_draw(dealer_upcard, true_count, Strategy::HILO) && state.hand.getScore() < 21) {
                        state.hand.draw(draw_card_from_deck(deck));
                    }
                }
            }
            play_dealer_hand(dealer, active_hands, deck);
        }

        int d_score = dealer.getScore();
        bool d_bj = dealer.is_blackjack();

        for (auto& state : active_hands) {
            total_hands_played++;
            int p_score = state.hand.getScore();
            bool p_bj = state.hand.is_blackjack();
            double hand_profit = 0.0;

            if (p_score > 21) { hand_profit = -state.bet; house_wins++; }
            else if (p_bj && !d_bj) { hand_profit = state.bet * 1.5; player_wins++; }
            else if (p_bj && d_bj) { hand_profit = 0.0; draws++; }
            else if (d_bj) { hand_profit = -state.bet; house_wins++; }
            else if (d_score > 21) { hand_profit = state.bet; player_wins++; }
            else if (p_score > d_score) { hand_profit = state.bet; player_wins++; }
            else if (d_score > p_score) { hand_profit = -state.bet; house_wins++; }
            else { hand_profit = 0.0; draws++; }

            bankroll += hand_profit;
            int outcome = (hand_profit > 0) ? 2 : ((hand_profit == 0) ? 1 : 0);
            outfile << total_hands_played << "\t" << state.initial_score << "\t" << dealer_upcard << "\t"
                << p_score << "\t" << d_score << "\t" << outcome << "\t" << true_count << "\t" << hand_profit << "\n";
        }
    }
    outfile.close();
    return { "Hi-Lo System", total_hands_played, player_wins, house_wins, draws, bankroll, bankroll / n, "log_hilo.txt" };
}
//KO counting sim
SimulationResult simulate_ko(int n) {
    double bankroll = 0.0;
    int base_unit = 10;
    int player_wins = 0, house_wins = 0, draws = 0, total_hands_played = 0;

    vector<int> deck = initialize_deck(6);
    ko_running_count = -20;
    ofstream outfile("log_ko.txt");
    if (!outfile.is_open()) { cerr << "Error: Could not open output file.\n"; exit(1); }
    outfile << "Hand\tP_Init\tD_Up\tP_Final\tD_Final\tWon\tCount\tProfit\n";

    for (int i = 0; i < n; ++i) {
        if (deck.size() < 78) { deck = initialize_deck(6); ko_running_count = -20; }

        int initial_ko_count = ko_running_count;
        double current_bet = base_unit;
        if (initial_ko_count >= -4) {
            int advantage = initial_ko_count - (-4) + 1;
            current_bet = base_unit * std::min(8, advantage);
        }

        vector<HandResult> active_hands;
        HandResult initial_hand = { addict(), current_bet, false, 0 };
        house dealer;

        deal_initial_cards(initial_hand, dealer, deck);
        initial_hand.initial_score = initial_hand.hand.getScore();
        int dealer_upcard = dealer.get_upcard();
        active_hands.push_back(initial_hand);

        if (!dealer.is_blackjack() && !initial_hand.hand.is_blackjack()) {
            process_splits(active_hands, current_bet, deck, dealer_upcard);

            for (auto& state : active_hands) {
                if (state.hand.should_double(dealer_upcard, initial_ko_count, Strategy::KO)) {
                    state.bet *= 2.0;
                    state.is_doubled = true;
                    state.hand.draw(draw_card_from_deck(deck));
                }
                else {
                    while (state.hand.should_draw(dealer_upcard, initial_ko_count, Strategy::KO) && state.hand.getScore() < 21) {
                        state.hand.draw(draw_card_from_deck(deck));
                    }
                }
            }
            play_dealer_hand(dealer, active_hands, deck);
        }

        int d_score = dealer.getScore();
        bool d_bj = dealer.is_blackjack();

        for (auto& state : active_hands) {
            total_hands_played++;
            int p_score = state.hand.getScore();
            bool p_bj = state.hand.is_blackjack();
            double hand_profit = 0.0;

            if (p_score > 21) { hand_profit = -state.bet; house_wins++; }
            else if (p_bj && !d_bj) { hand_profit = state.bet * 1.5; player_wins++; }
            else if (p_bj && d_bj) { hand_profit = 0.0; draws++; }
            else if (d_bj) { hand_profit = -state.bet; house_wins++; }
            else if (d_score > 21) { hand_profit = state.bet; player_wins++; }
            else if (p_score > d_score) { hand_profit = state.bet; player_wins++; }
            else if (d_score > p_score) { hand_profit = -state.bet; house_wins++; }
            else { hand_profit = 0.0; draws++; }

            bankroll += hand_profit;
            int outcome = (hand_profit > 0) ? 2 : ((hand_profit == 0) ? 1 : 0);

            outfile << total_hands_played << "\t" << state.initial_score << "\t" << dealer_upcard << "\t"
                << p_score << "\t" << d_score << "\t" << outcome << "\t" << initial_ko_count << "\t" << hand_profit << "\n";
        }
    }
    outfile.close();
    return { "KO System", total_hands_played, player_wins, house_wins, draws, bankroll, bankroll / n, "log_ko.txt" };
}

// ============================================================================
// MAIN EXECUTION
// ============================================================================
void print_results(const SimulationResult& res) {
    stringstream ss;
    ss << "\n=================================================================\n";
    ss << " STRATEGY: " << res.strategy_name << "\n";
    ss << "=================================================================\n";
    ss << "Total Hands Played : " << res.total_hands << "\n";
    ss << "Player Wins        : " << res.player_wins << " (" << fixed << setprecision(2) << (double)res.player_wins / res.total_hands * 100 << "%)\n";
    ss << "House Wins         : " << res.house_wins << " (" << fixed << setprecision(2) << (double)res.house_wins / res.total_hands * 100 << "%)\n";
    ss << "Draws              : " << res.draws << " (" << fixed << setprecision(2) << (double)res.draws / res.total_hands * 100 << "%)\n";
    ss << "-----------------------------------------------------------------\n";
    ss << "Net Profit         : $" << fixed << setprecision(2) << res.net_profit << "\n";
    ss << "EV per initial hand: $" << fixed << setprecision(4) << res.ev_per_hand << "\n";
    ss << "Output File        : " << res.log_file << "\n";
    ss << "=================================================================\n";

    cout << ss.str();
    string s = res.strategy_name + ".txt";
    ofstream summary_file(s);
    if (summary_file.is_open()) {
        summary_file << ss.str();
        summary_file.close();
        cout << "-> Summary statistics also saved to 'summary.txt'\n";
    }
    else {
        cerr << "Error: Could not write to summary.txt\n";
    }
}

int main() {
    int choice = 0;
    int iterations;
    cout << "Iterations: ";
    cin >> iterations;

    cout << "Select Card Counting Strategy:\n";
    cout << "1. Basic Strategy (Full Matrix)\n";
    cout << "2. Hi-Lo System\n";
    cout << "3. KO System\n";
    cout << "Enter choice (1-3): ";

    if (!(cin >> choice)) {
        cout << "Invalid input. Defaulting to Basic Strategy.\n";
        choice = 1;
    }

    cout << "\nExecuting simulation for " << iterations << " iterations...\n";

    SimulationResult result;
    if (choice == 2) {
        result = simulate_hilo(iterations);
    }
    else if (choice == 3) {
        result = simulate_ko(iterations);
    }
    else {
        result = simulate_basic(iterations);
    }

    print_results(result);

    return 0;
}