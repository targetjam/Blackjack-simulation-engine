#ifndef BLACKJACK_H
#define BLACKJACK_H

#include <vector>
#include <iostream>

using namespace std;

enum class Strategy { BASIC, HILO, KO };

class blackjack {
protected:
    int score;
    vector<int> cards;
    bool is_split_hand;
    bool is_soft_hand;

    void calculate_score() {
        score = 0;
        int aces = 0;

        for (int card : cards) {
            score += card;
            if (card == 11) aces++;
        }

        while (score > 21 && aces > 0) {
            score -= 10;
            aces--;
        }

        is_soft_hand = (aces > 0);
    }

public:
    blackjack() : score(0), is_split_hand(false), is_soft_hand(false) {}
    virtual ~blackjack() {}

    int getScore() { return score; }
    int getCardsDrawn() { return cards.size(); }
    void set_split() { is_split_hand = true; }
    bool is_soft() { return is_soft_hand; }

    bool is_blackjack() {
        return cards.size() == 2 && score == 21 && !is_split_hand;
    }

    void draw(int x) {
        cards.push_back(x);
        calculate_score();
    }

    int get_card(int i) {
        if (i >= 0 && i < cards.size()) return cards[i];
        return -1;
    }

    int pop_last_card() {
        if (cards.empty()) return 0;
        int val = cards.back();
        cards.pop_back();
        calculate_score();
        return val;
    }
};

class house : public blackjack {
public:
    bool should_draw() { return getScore() < 17; }
    int get_upcard() { return get_card(0); }
};

class addict : public blackjack {
public:
    bool should_draw(int dealer_upcard, int count_metric, Strategy strat) {
        int current_score = getScore();

        //Counting overrides
        if (strat == Strategy::KO) {
            if (current_score == 16 && dealer_upcard == 10 && count_metric >= -4) return false;
            if (current_score == 15 && dealer_upcard == 10 && count_metric >= 0) return false;
        }
        else if (strat == Strategy::HILO) {
            if (current_score == 16 && dealer_upcard == 10 && count_metric >= 0) return false;
            if (current_score == 15 && dealer_upcard == 10 && count_metric >= 4) return false;
        }

        //Matrix Baseline
        if (is_soft()) {
            if (current_score <= 17) return true;
            if (current_score == 18) {
                // Hit A,7 against 9, 10, A. Stand otherwise.
                return (dealer_upcard >= 9 && dealer_upcard <= 11);
            }
            return false; // Soft 19+ always stands
        }
        else {
            if (current_score <= 11) return true;
            if (current_score == 12) {
                // Stand against 4, 5, 6. Hit otherwise.
                return !(dealer_upcard >= 4 && dealer_upcard <= 6);
            }
            if (current_score >= 13 && current_score <= 16) {
                // Stand against 2 through 6. Hit otherwise (includes Surrender fallback).
                return !(dealer_upcard >= 2 && dealer_upcard <= 6);
            }
            return false; // Hard 17+ always stands
        }
    }

    bool should_double(int dealer_upcard, int count_metric, Strategy strat) {
        if (getCardsDrawn() != 2) return false;
        int current_score = getScore();

        // 1. Counting Deviations
        if (strat == Strategy::KO) {
            if (current_score == 10 && dealer_upcard == 10 && count_metric >= 4) return true;
        }
        else if (strat == Strategy::HILO) {
            if (current_score == 10 && dealer_upcard == 10 && count_metric >= 4) return true;
        }

        // 2. Strict Matrix Baseline
        if (is_soft()) {
            if (current_score == 13 || current_score == 14) return (dealer_upcard == 5 || dealer_upcard == 6);
            if (current_score == 15 || current_score == 16) return (dealer_upcard >= 4 && dealer_upcard <= 6);
            if (current_score == 17 || current_score == 18) return (dealer_upcard >= 3 && dealer_upcard <= 6);
            return false;
        }
        else {
            if (current_score == 11) return (dealer_upcard <= 10);
            if (current_score == 10) return (dealer_upcard <= 9);
            if (current_score == 9) return (dealer_upcard >= 3 && dealer_upcard <= 6);
            return false;
        }
    }

    bool split_cards(int dealer_upcard) {
        if (getCardsDrawn() == 2) {
            int c1 = get_card(0);
            int c2 = get_card(1);

            if (c1 == 11 && c2 == 11) return true; // Always split Aces
            if (c1 != c2) return false;

            if (c1 == 10 || c1 == 5) return false; // Never split 10s or 5s
            if (c1 == 9) return (dealer_upcard <= 6 || dealer_upcard == 8 || dealer_upcard == 9);
            if (c1 == 8) return true;              // Always split 8s
            if (c1 == 7) return (dealer_upcard <= 7);
            if (c1 == 6) return (dealer_upcard <= 6);
            if (c1 == 4) return (dealer_upcard == 5 || dealer_upcard == 6);
            if (c1 == 2 || c1 == 3) return (dealer_upcard <= 7);
        }
        return false;
    }
};

#endif