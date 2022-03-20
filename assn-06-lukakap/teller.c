#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"

#include "branch.h"

int AccountNum_GetBranchID (AccountNumber x);

/*
 * deposit money into an account
 */
int
Teller_DoDeposit(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoDeposit(account 0x%"PRIx64" amount %"PRId64")\n",
                accountNum, amount));

  Account *account = Account_LookupByNumber(bank, accountNum);

  if (account == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  BranchID branch = AccountNum_GetBranchID(accountNum);
  sem_wait(&(account->accountLock));
  sem_wait(&(bank->branches[branch].brancLock));

  Account_Adjust(bank,account, amount, 1);

  sem_post(&(account->accountLock));
  sem_post(&(bank->branches[branch].brancLock));

  return ERROR_SUCCESS;
}

/*
 * withdraw money from an account
 */
int
Teller_DoWithdraw(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoWithdraw(account 0x%"PRIx64" amount %"PRId64")\n",
                accountNum, amount));

  Account *account = Account_LookupByNumber(bank, accountNum);

  if (account == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  BranchID branch = AccountNum_GetBranchID(accountNum);
  sem_wait(&(account->accountLock));
  sem_wait(&(bank->branches[branch].brancLock));


  if (amount > Account_Balance(account)) {
    sem_post(&(account->accountLock));
    sem_post(&(bank->branches[branch].brancLock));
    return ERROR_INSUFFICIENT_FUNDS;
  }

  Account_Adjust(bank,account, -amount, 1);
  sem_post(&(account->accountLock));
  sem_post(&(bank->branches[branch].brancLock));

  return ERROR_SUCCESS;
}

/*
 * do a tranfer from one account to another account
 */
int
Teller_DoTransfer(Bank *bank, AccountNumber srcAccountNum,
                  AccountNumber dstAccountNum,
                  AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoTransfer(src 0x%"PRIx64", dst 0x%"PRIx64
                ", amount %"PRId64")\n",
                srcAccountNum, dstAccountNum, amount));

  Account *srcAccount = Account_LookupByNumber(bank, srcAccountNum);
  if (srcAccount == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  Account *dstAccount = Account_LookupByNumber(bank, dstAccountNum);
  if (dstAccount == NULL) {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  if (amount > Account_Balance(srcAccount)) {
    return ERROR_INSUFFICIENT_FUNDS;
  }

  if (srcAccountNum == dstAccountNum) {
    return ERROR_SUCCESS;
  }

  BranchID branchSrc = AccountNum_GetBranchID(srcAccountNum);
  BranchID branchDst = AccountNum_GetBranchID(dstAccountNum);

  if (srcAccountNum >= dstAccountNum) {
    sem_wait(&(dstAccount->accountLock));
    sem_wait(&(srcAccount->accountLock));
  } else {
    sem_wait(&(srcAccount->accountLock));
    sem_wait(&(dstAccount->accountLock));
  }
  
  int updateBranch = !Account_IsSameBranch(srcAccountNum, dstAccountNum);

  if (updateBranch) {
    if (srcAccountNum >= dstAccountNum) {
      sem_wait(&(bank->branches[branchDst].brancLock));
      sem_wait(&(bank->branches[branchSrc].brancLock));
    }
    else {
      sem_wait(&(bank->branches[branchSrc].brancLock));
      sem_wait(&(bank->branches[branchDst].brancLock));
    }
  }



  Account_Adjust(bank, srcAccount, -amount, updateBranch);
  Account_Adjust(bank, dstAccount, amount, updateBranch);


  if (updateBranch) {
    sem_post(&(bank->branches[branchSrc].brancLock));
    sem_post(&(bank->branches[branchDst].brancLock));
  }

  sem_post(&(srcAccount->accountLock));
  sem_post(&(dstAccount->accountLock));

  return ERROR_SUCCESS;
}
