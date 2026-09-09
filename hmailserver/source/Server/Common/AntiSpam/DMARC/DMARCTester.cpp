#include "StdAfx.h"
#include "DMARCTester.h"
#include "DMARCEvaluator.h"
namespace HM { void DMARCTester::Test() { DMARCRecord r; if (!DMARCRecord::Parse("v=DMARC1; p=reject", r)) throw; if (r.GetPolicy()!=DMARCRecord::Policy::Reject) throw; if (!DMARCEvaluator::IsAligned("mail.example.com","example.com",DMARCRecord::Alignment::Relaxed)) throw; } }
