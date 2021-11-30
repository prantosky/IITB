//
// Created by Pranav on 09-11-2020.
//

#include "BlockChain.h"

BlockChain::~BlockChain() {
	selfBlocksGenerated.close();
	stopped = true;
}

BlockChain::BlockChain() {
	mInterarrivalTime = 5;
	mNodeHashPower = 0.01;

	random_device rd;
	mt19937 rnd_gen(rd());
	exponential_distribution<double> distribution(1.0);

	mDistribution = distribution;
	rndGen = rnd_gen;
}

BlockChain::BlockChain(long interarrival_Time, double nodeHashPower)
	: mInterarrivalTime(interarrival_Time), mNodeHashPower(nodeHashPower) {
	random_device rd;
	mt19937 rnd_gen(rd());
	exponential_distribution<double> distribution(1.0);

	mDistribution = distribution;
	rndGen = rnd_gen;
}

void BlockChain::init(const string &ip, const int port, bool wait) {
	selfBlocksGenerated.open(ip + "_" + to_string(port) + "_" +
								 to_string(mInterarrivalTime) + ".blocks",
							 ofstream::out);
	if (not selfBlocksGenerated.is_open()) {
		exit(EXIT_FAILURE);
	}
	cout << "BlockChain::init: Initializing BlockChain" << endl;
	connection.initialize();
	string tableName = "chain_" + ip + "_" + to_string(port) + "_" +
					   to_string(mInterarrivalTime);
	replace(tableName.begin(), tableName.end(), '.', '_');
	connection.dropTable(tableName);
	connection.createTable(tableName);
	(new thread([this]() {
		this->_keepCheckingBlocksForInsertion();
	}))->detach();

	(new thread([this, wait]() { this->_createBlock(wait); }))->detach();
}

void BlockChain::_keepCheckingBlocksForInsertion() {
	cout << "BlockChain::_keepCheckingBlocksForInsertion: Thread Started"
		 << endl;
	while (not(stopped and mWaitingBlocks.empty())) {
		if (not mWaitingBlocks.empty()) {
			{
				unique_lock<mutex> lock(mWaitingBlocksQueueMutex);
				cv.wait(lock, [this]() {
					return (not mWaitingBlocks.empty()) || stopped;
				});
				if (stopped and mWaitingBlocks.empty()) {
					return;
				}
				//                cout <<
				//                "BlockChain::_keepCheckingBlocksForInsertion:
				//                mWaiting Blocks before: " <<
				//                mWaitingBlocks.size()
				//                     << "\t endBlocks: " << endBlocks.size()
				//                     << endl;
				for (auto items = mWaitingBlocks.begin();
					 items != mWaitingBlocks.end(); items++) {
					auto waitingBlock = items->second.first;
					if (waitingBlock.getPreviousBlockHash() ==
						mGenesisBlockHash) {
						{
							unique_lock lck(endPointMutex);
							endBlocks.insert({waitingBlock.getThisBlockHash(),
											  waitingBlock});
						}
						if (waitingBlock.getPreviousBlockHash() ==
							getMinigOnBlock()) {
							setMiningOnBlock(waitingBlock.getThisBlockHash());
						}

						if (_addBlock(waitingBlock)) {
							//                            cout <<
							//                            "BlockChain::_keepCheckingBlocksForInsertion:
							//                            Adding Genesis block "
							//                            << waitingBlock
							//                                 << endl;
						}
					}
					auto it = cache.find(waitingBlock.getPreviousBlockHash());
					if (it != cache.end()) {
						if (waitingBlock.getPreviousBlockHash() ==
							getMinigOnBlock()) {
							setMiningOnBlock(waitingBlock.getThisBlockHash());
						}
						if (_addBlock(waitingBlock)) {
							//                            cout <<
							//                            "BlockChain::_keepCheckingBlocksForInsertion:
							//                            Adding block " <<
							//                            waitingBlock
							//                                 << endl;
						}
					}
				}

				//                cout <<
				//                "BlockChain::_keepCheckingBlocksForInsertion:
				//                mWaiting Blocks after: " <<
				//                mWaitingBlocks.size()
				//                     << "\t endBlocks: " << endBlocks.size()
				//                     << endl;
			}
		}
		this_thread::sleep_for(chrono::milliseconds(100));
		//        cout << "BlockChain::_keepCheckingBlocksForInsertion: Waiting"
		//        << endl;
	}
}

bool BlockChain::_addBlock(Block &blk) {
	auto prevBlock = checkForPrevious(blk);
	if (prevBlock.has_value()) {
		blk.setHeight(prevBlock->getHeight() + 1);
		cache.insert(make_pair(blk.getThisBlockHash(), blk));
		connection.insertBlock(blk);
		_updateEndBlocks(blk);
		_removeFromWaitingQueue(blk);
		return true;
	}
	return false;
}

void BlockChain::_updateEndBlocks(Block &blk) {
	unique_lock lck(endPointMutex);
	if (endBlocks.empty()) {
		endBlocks.insert({blk.getThisBlockHash(), blk});
		return;
	}
	auto self_it = endBlocks.find(blk.getThisBlockHash());
	auto prev_it = endBlocks.find(blk.getPreviousBlockHash());

	if (self_it != endBlocks.end()) {
		auto it = cache.find(blk.getPreviousBlockHash());
		if (it != cache.end()) {
			endBlocks.insert(make_pair(blk.getThisBlockHash(), blk));
		}
	} else if (prev_it != endBlocks.end()) {
		endBlocks.erase(prev_it);
		endBlocks.insert(make_pair(blk.getThisBlockHash(), blk));
	}
}

double BlockChain::_getRandomExpNumber() { return mDistribution(rndGen); }

// Removes the given block from the waiting queue.
// Does not acquire lock on waiting queue.
// Assusmes the function caller has already acquired the lock on waiting queue.
void inline BlockChain::_removeFromWaitingQueue(const Block &blk) {
	auto it = mWaitingBlocks.find(blk.getThisBlockHash());
	if (it != mWaitingBlocks.end()) {
		mWaitingBlocks.erase(it);
	}
}

bool BlockChain::_validateBlock(const Block &blk) {
	int currentTimeStamp = Helper::getSystemTimeStamp();
	int blockTimeStamp = blk.getTimeStamp();
	if (abs(currentTimeStamp - blockTimeStamp) > 60 * 60) {
		cerr << "BlockChain::_validateBlock: Invalid timestamp for block "
			 << blk << endl;
		return false;
	}
	return true;
}

optional<Block> BlockChain::checkForPrevious(Block &blk) {
	if (blk.getPreviousBlockHash() == mGenesisBlockHash) {
		Block blk1;
		blk1.setHeight(0);
		return blk1;
	}
	string prevBlockHash = blk.getPreviousBlockHash();
	auto it = cache.find(prevBlockHash);
	if (it != cache.end()) {
		return it->second;
	} else {
		auto prevBlock = connection.searchBlock(blk);
		if (prevBlock.has_value()) {
			return *prevBlock;
		} else {
			cerr << "BlockChain::addBlock: No such previous block found "
					"for block "
				 << blk << endl;
			return nullopt;
		}
	}
}

bool BlockChain::addToQueue(const Block &blk) {
	if (not _validateBlock(blk)) {
		cerr << "BlockChain::addToQueue: Invalid Block: " << blk << endl;
		return false;
	}

	// Block already inserted in BlockChain
	auto it = cache.find(blk.getThisBlockHash());
	if (it != cache.end()) {
		cout << "BlockChain::addToQueue: Block already in BlockChain: " << blk
			 << endl;
		cv.notify_one();
		return false;
	}
	bool retVal = false;

	{
		unique_lock lock(mWaitingBlocksQueueMutex);
		auto prevBlockInChain = cache.find(blk.getPreviousBlockHash());
		auto prevBlockInQueue = mWaitingBlocks.find(blk.getPreviousBlockHash());
		auto curr_block = mWaitingBlocks.find(blk.getThisBlockHash());

		if (curr_block != mWaitingBlocks.end()) {
			// Incoming block already exists in the waiting queue.
			//            cout << "BlockChain::addToQueue: Case 1" << endl;
		} else if (curr_block == mWaitingBlocks.end() and
				   prevBlockInChain == cache.end() and
				   prevBlockInQueue == mWaitingBlocks.end()) {
			// Incoming block as well as its previous block does not exists.
			// Add to the sync queue and waiting queue
			//            cout << "BlockChain::addToQueue: Case 2" << endl;
			mWaitingBlocks.insert({blk.getThisBlockHash(), {blk, false}});
			if (blk.getPreviousBlockHash() != mGenesisBlockHash) {
				pushSyncBlock(blk);
			}
		} else if (curr_block == mWaitingBlocks.end() and
				   (prevBlockInQueue != mWaitingBlocks.end() or
					prevBlockInChain != cache.end())) {
			// Incoming block does not exist but the previous block exists.
			//            cout << "BlockChain::addToQueue: Case 3" << endl;
			mWaitingBlocks.insert({blk.getThisBlockHash(), {blk, false}});
			retVal = true;
		} else {
			//            cout << "BlockChain::addToQueue: Case 4" << endl;
		}
	}
	cv.notify_one();
	return retVal;
}

optional<Block> BlockChain::_getMaxHeightBlock() {
	unique_lock lock(endPointMutex);
	Block maxPrevBlock(mGenesisBlockHash, "0000", -1);
	if (endBlocks.empty()) {
		return nullopt;
	}
	for (auto &endBlock : endBlocks) {
		Block blk = endBlock.second;
		if (blk.getHeight() > maxPrevBlock.getHeight()) {
			maxPrevBlock = blk;
		} else if (blk.getHeight() == maxPrevBlock.getHeight()) {
			if (blk.getTimeStamp() > maxPrevBlock.getTimeStamp()) {
				maxPrevBlock = blk;
			}
		}
	}
	return maxPrevBlock;
}

// Creates a new Block and adds it to the waiting queue.
void BlockChain::_createBlock(bool wait) {
	setMiningOnBlock(const_cast<string &>(mGenesisBlockHash));
	bool genesisBlockCreated = false;
	double globalLambda = 1.0 / static_cast<double>(mInterarrivalTime);
	double lambda = mNodeHashPower * (globalLambda / 100.0);
	int waitingTime = 3000;

	if (wait) {
		this_thread::sleep_for(chrono::seconds(3 * mInterarrivalTime));
	}
	long time_quantum = 100;

	while (true) {
		{
			unique_lock lck1(mWaitingBlocksQueueMutex);
			unique_lock lck2(mSyncBlockQueueMutex);
			if (mWaitingBlocks.size() <= 1 and mSyncBlockQueue.empty()) {
				break;
			} else {
				genesisBlockCreated = true;
			}
		}
		this_thread::sleep_for(chrono::milliseconds(time_quantum));
	}
	if (not cache.empty()) {
		genesisBlockCreated = true;
	}
	string prevMiningOnHash = mGenesisBlockHash;
	string miningOnHash = getMinigOnBlockUpdated();

	cout << "BlockChain::_createBlock: STARTING TO MINE" << endl;
	while (not stopped) {
		//        auto prevBlock = _getMaxHeightBlock();
		//        if (prevBlock.has_value()) {
		//            setMiningOnBlock(prevBlock.value().getThisBlockHash());
		//            cout << "BlockChain::_createBlock: WILL BE MINING ON " <<
		//            prevBlock.value() << endl;
		//        }

		miningOnHash = getMinigOnBlockUpdated();
		while (genesisBlockCreated and prevMiningOnHash == miningOnHash) {
			miningOnHash = getMinigOnBlockUpdated();
		}

		waitingTime = static_cast<int>(_getRandomExpNumber() / lambda);
		std::this_thread::sleep_for(std::chrono::milliseconds(waitingTime));

		{
			Block retBlk("0", "0", 0);
			if (not genesisBlockCreated) {
				Block blk(mGenesisBlockHash, Helper::getRandomString(4),
						  Helper::getSystemTimeStamp());
				blk.setHeight(1);
				//                cout << "BlockChain::_createBlock: " << blk <<
				//                endl;
				cout << "Created Block: " << blk << endl;
				_addBlock(blk);
				if (selfBlocksGenerated.is_open()) {
					selfBlocksGenerated << blk << endl;
				}
				setMiningOnBlock(blk.getThisBlockHash());
				pushBroadcastBlock(blk);
				genesisBlockCreated = true;
			} else if (_compareWithMaxHeightBlock(miningOnHash, retBlk)) {
				// Block with greater height not present in BlockChain
				Block blk(getMinigOnBlock(), Helper::getRandomString(4),
						  Helper::getSystemTimeStamp());
				blk.setHeight((retBlk.getHeight() + 1));
				cout << "Created Block: " << blk << endl;
				prevMiningOnHash = miningOnHash;
				addToQueue(blk);
				if (selfBlocksGenerated.is_open()) {
					selfBlocksGenerated << blk << endl;
				}
				pushBroadcastBlock(blk);
			} else {
				//                cout << "BlockChain::_createBlock: Block has
				//                been reset: " << retBlk << endl; cout <<
				//                "BlockChain::_createBlock: Block has been
				//                reset: " << miningOnHash << endl; cout <<
				//                "BlockChain::_createBlock: Block has been
				//                reset: " << getMinigOnBlock() << endl;
			}
		}
	}
}

bool BlockChain::_compareWithMaxHeightBlock(const string &hash, Block &retBlk) {
	unique_lock lck(mWaitingBlocksQueueMutex);
	unique_lock lock(endPointMutex);

	Block maxPrevBlock(mGenesisBlockHash, "0000", -1);
	if (endBlocks.empty()) {
		return false;
	}
	for (auto &endBlock : endBlocks) {
		Block blk = endBlock.second;
		if (blk.getHeight() > maxPrevBlock.getHeight()) {
			maxPrevBlock = blk;
		} else if (blk.getHeight() == maxPrevBlock.getHeight()) {
			if (blk.getTimeStamp() < maxPrevBlock.getTimeStamp()) {
				maxPrevBlock = blk;
			}
		}
	}
	retBlk = maxPrevBlock;
	return maxPrevBlock.getThisBlockHash() == hash;
}

string BlockChain::getMinigOnBlockUpdated() {
	//    cout << "BlockChain::getMiningBlock: get mining on block" << endl;
	unique_lock l1(mWaitingBlocksQueueMutex);
	unique_lock l2(endPointMutex);
	unique_lock l3(mResetMutex);
	return mMiningOnBlock;
}

string BlockChain::getMinigOnBlock() {
	//    cout << "BlockChain::getMiningBlock: get mining on block" << endl;
	unique_lock l3(mResetMutex);
	return mMiningOnBlock;
}

void BlockChain::setMiningOnBlock(const string &newBlk) {
	unique_lock lck(mResetMutex);
	mMiningOnBlock = newBlk;
}

Block BlockChain::popBroadcastBlock() {
	unique_lock lck(mBroadcastQueueMutex);
	while (mBroadcastQueue.empty()) {
		mBroadcastCV.wait(lck);
	}
	auto retVal = mBroadcastQueue.front();
	mBroadcastQueue.pop();
	return retVal;
}

void BlockChain::pushBroadcastBlock(const Block &blk) {
	lock_guard lck(mBroadcastQueueMutex);
	bool wake = mBroadcastQueue.empty();
	mBroadcastQueue.push(blk);
	if (wake) {
		mBroadcastCV.notify_one();
	}
}

Block BlockChain::popSyncBlock() {
	unique_lock lck(mSyncBlockQueueMutex);
	while (mSyncBlockQueue.empty()) {
		mSyncBlockCV.wait(lck);
	}
	auto retVal = mSyncBlockQueue.front();
	mSyncBlockQueue.pop();
	return retVal;
}

void BlockChain::pushSyncBlock(const Block &blk) {
	lock_guard lck(mSyncBlockQueueMutex);
	bool wake = mSyncBlockQueue.empty();
	mSyncBlockQueue.push(blk);
	if (wake) {
		mSyncBlockCV.notify_one();
	}
}