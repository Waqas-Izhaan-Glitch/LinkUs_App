#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <ctime>
#include <iomanip>
#include <limits>

using namespace std;

// 1. Utility Functions

string formatTime(time_t timestamp) {
    char buffer[80];
    tm* timeInfo = localtime(&timestamp);
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M", timeInfo);
    return buffer;
}

string toLower(string str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

void pause() {
    cout << "\nPress Enter to continue...";
    cin.get();
}

int readInt(const string& message) {
    int value;
    while (true) {
        cout << message;
        if (cin >> value) {
            cin.ignore();
            return value;
        }
        cout << "Invalid input. Please enter a number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string readLine(const string& message) {
    string line;
    cout << message;
    getline(cin, line);
    return line;
}

// 2. Tweet — stores tweet id, author, content, timestamp, likes

struct Tweet {
    int    id;
    int    authorId;
    string content;
    time_t timestamp;
    int    likes;

    // new tweet
    Tweet(int id, int authorId, const string& content)
        : id(id), authorId(authorId), content(content),
          timestamp(time(nullptr)), likes(0) {}

    // loaded from file
    Tweet(int id, int authorId, const string& content,
          time_t timestamp, int likes)
        : id(id), authorId(authorId), content(content),
          timestamp(timestamp), likes(likes) {}
};

// 3. User — stores id, username, bio, following list, liked tweets

struct User {
    int    id;
    string username;
    string bio;

    unordered_set<int> following;
    unordered_set<int> likedTweets;

    User(int id, const string& username, const string& bio)
        : id(id), username(username), bio(bio) {
        following.insert(id); // self-follow
    }

    bool isFollowing(int userId)  const { return following.count(userId) > 0;   }
    bool hasLiked(int tweetId)    const { return likedTweets.count(tweetId) > 0; }
};

// 4. Storage — saves and loads all data to/from a text file

class Storage {
    string filename;

    // joins a set of ints into a comma-separated string
    string joinSet(const unordered_set<int>& values) {
        string result;
        for (int v : values) {
            if (!result.empty()) result += ",";
            result += to_string(v);
        }
        return result;
    }

    // splits a comma-separated string back into a set of ints
    unordered_set<int> splitSet(const string& str) {
        unordered_set<int> result;
        stringstream ss(str);
        string token;
        while (getline(ss, token, ','))
            if (!token.empty()) result.insert(stoi(token));
        return result;
    }

public:
    explicit Storage(const string& filename) : filename(filename) {}

    // writes users then tweets to file
    void save(const vector<User>& users, const vector<Tweet>& tweets) {
        ofstream file(filename);
        if (!file) return;

        file << users.size() << "\n";
        for (const User& u : users) {
            file << u.id << "|" << u.username << "|" << u.bio << "|"
                 << joinSet(u.following) << "|" << joinSet(u.likedTweets) << "\n";
        }

        file << tweets.size() << "\n";
        for (const Tweet& t : tweets) {
            string safe = t.content;
            replace(safe.begin(), safe.end(), '|', '~'); // escape pipe chars
            file << t.id << "|" << t.authorId << "|" << safe << "|"
                 << t.timestamp << "|" << t.likes << "\n";
        }
    }

    // reads users and tweets from file, updates id counters
    void load(vector<User>& users, vector<Tweet>& tweets,
              int& nextUserId, int& nextTweetId) {
        ifstream file(filename);
        if (!file) return;

        users.clear();
        tweets.clear();

        int userCount = 0;
        file >> userCount;
        file.ignore();

        for (int i = 0; i < userCount; i++) {
            string line;
            getline(file, line);
            stringstream ss(line);

            string idStr, username, bio, followingStr, likedStr;
            getline(ss, idStr,        '|');
            getline(ss, username,     '|');
            getline(ss, bio,          '|');
            getline(ss, followingStr, '|');
            getline(ss, likedStr);

            if (idStr.empty()) continue;

            int id = stoi(idStr);
            User user(id, username, bio);
            user.following   = splitSet(followingStr);
            user.likedTweets = splitSet(likedStr);
            user.following.insert(id); // always keep self-follow

            users.push_back(user);
            nextUserId = max(nextUserId, id + 1);
        }

        int tweetCount = 0;
        file >> tweetCount;
        file.ignore();

        for (int i = 0; i < tweetCount; i++) {
            string line;
            getline(file, line);
            stringstream ss(line);

            string idStr, authorStr, content, timeStr, likesStr;
            getline(ss, idStr,    '|');
            getline(ss, authorStr,'|');
            getline(ss, content,  '|');
            getline(ss, timeStr,  '|');
            getline(ss, likesStr);

            if (idStr.empty() || authorStr.empty() || timeStr.empty() || likesStr.empty())
                continue;

            replace(content.begin(), content.end(), '~', '|'); // restore pipe chars

            int    id        = stoi(idStr);
            int    authorId  = stoi(authorStr);
            time_t timestamp = (time_t)stoll(timeStr);
            int    likes     = stoi(likesStr);

            tweets.emplace_back(id, authorId, content, timestamp, likes);
            nextTweetId = max(nextTweetId, id + 1);
        }
    }
};

// 5. Twitter — main logic: users, tweets, follow, feed, search, like, delete

class Twitter {
    vector<User>  users;
    vector<Tweet> tweets;

    int nextUserId  = 1;
    int nextTweetId = 1;

    Storage storage;

    // helpers to find by id or name
    User*  findUser(int id) {
        for (auto& u : users)  if (u.id == id) return &u;
        return nullptr;
    }
    Tweet* findTweet(int id) {
        for (auto& t : tweets) if (t.id == id) return &t;
        return nullptr;
    }
    User*  findUserByName(const string& name) {
        for (auto& u : users)  if (u.username == name) return &u;
        return nullptr;
    }

    // prints a single tweet in a consistent format
    void printTweet(const Tweet& tweet, const string& username) const {
        cout << "\n_______________________________\n";
        cout << "User     : @" << username          << "\n";
        cout << "Tweet ID : "  << tweet.id           << "\n";
        cout << "Content  : "  << tweet.content      << "\n";
        cout << "Likes    : "  << tweet.likes         << "\n";
        cout << "Posted   : "  << formatTime(tweet.timestamp) << "\n";
        cout << "_________________________________\n";
        //shift + underscore = to get the line 
    }

public:
    explicit Twitter(const string& filename) : storage(filename) {
        storage.load(users, tweets, nextUserId, nextTweetId);
    }

    // saves all data on exit
    ~Twitter() {
        storage.save(users, tweets);
    }

    // 5a. Create User — validates username, checks for duplicates
    void createUser() {
        string username = readLine("\nEnter username: ");

        if (username.empty()) {
            cout << "Username cannot be empty.\n";
            return;
        }
        if (username.length() < 3) {
            cout << "Username must be at least 3 characters.\n";
            return;
        }
        if (findUserByName(username)) {
            cout << "Username already taken.\n";
            return;
        }

        string bio = readLine("Enter bio: ");

        users.emplace_back(nextUserId, username, bio);
        cout << "User created. ID: " << nextUserId << "\n";
        nextUserId++;
    }

    // 5b. Post Tweet — checks user exists and content length
    void postTweet() {
        int userId = readInt("\nEnter user ID: ");

        User* user = findUser(userId);
        if (!user) { cout << "User not found.\n"; return; }

        string content = readLine("Enter tweet: ");

        if (content.empty()) {
            cout << "Tweet cannot be empty.\n";
            return;
        }
        if (content.length() > 280) {
            cout << "Tweet exceeds 280 characters.\n";
            return;
        }

        tweets.emplace_back(nextTweetId, userId, content);
        cout << "Tweet posted. ID: " << nextTweetId << "\n";
        nextTweetId++;
    }

    // 5c. Follow User — prevents self-follow and duplicate follows
    void followUser() {
        int followerId = readInt("\nEnter your user ID: ");
        int followeeId = readInt("Enter user ID to follow: ");

        if (followerId == followeeId) {
            cout << "You cannot follow yourself.\n";
            return;
        }

        User* follower = findUser(followerId);
        User* followee = findUser(followeeId);

        if (!follower || !followee) {
            cout << "Invalid user ID.\n";
            return;
        }
        if (follower->isFollowing(followeeId)) {
            cout << "Already following @" << followee->username << ".\n";
            return;
        }

        follower->following.insert(followeeId);
        cout << "Now following @" << followee->username << ".\n";
    }

    // 5d. Unfollow User — prevents unfollowing self
    void unfollowUser() {
        int followerId = readInt("\nEnter your user ID: ");
        int followeeId = readInt("Enter user ID to unfollow: ");

        if (followerId == followeeId) {
            cout << "You cannot unfollow yourself.\n";
            return;
        }

        User* follower = findUser(followerId);
        User* followee = findUser(followeeId);

        if (!follower) { cout << "User not found.\n"; return; }

        if (!follower->isFollowing(followeeId)) {
            cout << "You are not following this user.\n";
            return;
        }

        follower->following.erase(followeeId);
        cout << "Unfollowed @" << (followee ? followee->username : to_string(followeeId)) << ".\n";
    }

    // 5e. Show Feed — tweets from followed users, newest first, max 10
    void showFeed() {
        int userId = readInt("\nEnter user ID: ");

        User* user = findUser(userId);
        if (!user) { cout << "User not found.\n"; return; }

        vector<Tweet> feed;
        for (const Tweet& t : tweets)
            if (user->isFollowing(t.authorId))
                feed.push_back(t);

        sort(feed.begin(), feed.end(),
             [](const Tweet& a, const Tweet& b) {
                 return a.timestamp > b.timestamp;
             });

        if (feed.empty()) { cout << "No tweets in feed.\n"; return; }

        cout << "\n=={ NEWS FEED }== \n";

        int shown = 0;
        for (const Tweet& t : feed) {
            if (shown++ == 10) break;
            User* author = findUser(t.authorId);
            printTweet(t, author ? author->username : "Unknown");
        }
    }

    // 5f. Like / Unlike — toggles like on a tweet
    void likeTweet() {
        int userId  = readInt("\nEnter user ID: ");
        int tweetId = readInt("Enter tweet ID: ");

        User*  user  = findUser(userId);
        Tweet* tweet = findTweet(tweetId);

        if (!user || !tweet) {
            cout << "Invalid user or tweet ID.\n";
            return;
        }

        if (user->hasLiked(tweetId)) {
            user->likedTweets.erase(tweetId);
            if (tweet->likes > 0) tweet->likes--;
            cout << "Like removed.\n";
        } else {
            user->likedTweets.insert(tweetId);
            tweet->likes++;
            cout << "Tweet liked. Total likes: " << tweet->likes << "\n";
        }
    }

    // 5g. Search Tweets — case-insensitive keyword search
    void searchTweets() {
        string keyword = readLine("\nEnter keyword: ");

        if (keyword.empty()) { cout << "Keyword cannot be empty.\n"; return; }

        keyword = toLower(keyword);
        bool found = false;

        for (const Tweet& t : tweets) {
            if (toLower(t.content).find(keyword) != string::npos) {
                User* author = findUser(t.authorId);
                printTweet(t, author ? author->username : "Unknown");
                found = true;
            }
        }

        if (!found) cout << "No matching tweets found.\n";
    }

    // 5h. View Profile — shows stats and last 5 tweets of a user
    void viewProfile() {
        int userId = readInt("\nEnter user ID: ");

        User* user = findUser(userId);
        if (!user) { cout << "User not found.\n"; return; }

        int followers  = 0;
        int tweetCount = 0;

        for (const User& u : users)
            if (u.id != userId && u.isFollowing(userId))
                followers++;

        vector<const Tweet*> myTweets;
        for (const Tweet& t : tweets)
            if (t.authorId == userId) {
                tweetCount++;
                myTweets.push_back(&t);
            }

        cout << "\n=={ PROFILE }==\n";
        cout << "Username  : @" << user->username                   << "\n";
        cout << "Bio       : "  << user->bio                        << "\n";
        cout << "Tweets    : "  << tweetCount                       << "\n";
        cout << "Followers : "  << followers                        << "\n";
        cout << "Following : "  << (user->following.size() - 1)     << "\n"; // exclude self

        // show last 5 tweets
        sort(myTweets.begin(), myTweets.end(),
             [](const Tweet* a, const Tweet* b) {
                 return a->timestamp > b->timestamp;
             });

        if (!myTweets.empty()) {
            cout << "\n--- Recent Tweets ---\n";
            int shown = 0;
            for (const Tweet* t : myTweets) {
                if (shown++ == 5) break;
                printTweet(*t, user->username);
            }
        }
    }

    // 5i. Show All Users — lists every user with id and username
    void showUsers() {
        if (users.empty()) { cout << "No users found.\n"; return; }

        cout << "\n=={ ALL USERS }==\n";
        for (const User& u : users) {
            cout << "\nID       : " << u.id           << "\n";
            cout << "Username : @" << u.username      << "\n";
            cout << "Bio      : "  << u.bio           << "\n";
        }
    }

    // 5j. Delete Tweet — only the author can delete their tweet
    void deleteTweet() {
        int userId  = readInt("\nEnter user ID: ");
        int tweetId = readInt("Enter tweet ID: ");

        auto it = find_if(tweets.begin(), tweets.end(),
                          [tweetId](const Tweet& t) { return t.id == tweetId; });

        if (it == tweets.end()) { cout << "Tweet not found.\n"; return; }

        if (it->authorId != userId) {
            cout << "You can only delete your own tweets.\n";
            return;
        }

        tweets.erase(it);

        // remove this tweet from all users' liked sets
        for (User& u : users)
            u.likedTweets.erase(tweetId);

        cout << "Tweet deleted.\n";
    }
};

// 6. Menu — prints all available options

void printMenu() {
    cout << "\n=={ MINI TWITTER }==\n";
    cout << "1. Create User\n";
    cout << "2. Post Tweet\n";
    cout << "3. Follow User\n";
    cout << "4. Unfollow User\n";
    cout << "5. View Feed\n";
    cout << "6. Like Tweet\n";
    cout << "7. Search Tweets\n";
    cout << "8. View Profile\n";
    cout << "9. Show All Users\n";
    cout << "10. Delete Tweet\n";
    cout << "0. Exit\n";
    cout << "\nEnter choice: ";
}

// 7. Main — program entry, menu loop

int main() {
    Twitter twitter("twitter_data.txt");

    while (true) {
        printMenu();

        int choice = readInt("");

        switch (choice) {
            case 1:  twitter.createUser();   break;
            case 2:  twitter.postTweet();    break;
            case 3:  twitter.followUser();   break;
            case 4:  twitter.unfollowUser(); break;
            case 5:  twitter.showFeed();     break;
            case 6:  twitter.likeTweet();    break;
            case 7:  twitter.searchTweets(); break;
            case 8:  twitter.viewProfile();  break;
            case 9:  twitter.showUsers();    break;
            case 10: twitter.deleteTweet();  break;
            case 0:
                cout << "\nData saved. Goodbye!\n";
                return 0;
            default:
                cout << "Invalid option. Try again.\n";
        }

        pause();
    }

    return 0;
}