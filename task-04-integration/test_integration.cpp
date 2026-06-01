// test_integration.cpp — 集成测试 (4 tests): JSON→AST→HE→解密→验证

#include "expr_parser.h"
#include "he_evaluator.h"
#include <iostream>
#include <vector>
#include <cmath>
using namespace lbcrypto; using namespace mpc;

CryptoContext<DCRTPoly> setup_ckks(uint32_t md=4) {
    CCParams<CryptoContextCKKSRNS> p; p.SetMultiplicativeDepth(md); p.SetScalingModSize(50); p.SetBatchSize(16);
    auto cc=GenCryptoContext(p); cc->Enable(PKE); cc->Enable(KEYSWITCH); cc->Enable(LEVELEDSHE); cc->Enable(ADVANCEDSHE);
    return cc;
}
Ciphertext<DCRTPoly> encrypt(CryptoContext<DCRTPoly> cc, PublicKey<DCRTPoly> pk, const std::vector<double>& d) { return cc->Encrypt(pk, cc->MakeCKKSPackedPlaintext(d)); }
std::vector<double> decrypt(CryptoContext<DCRTPoly> cc, PrivateKey<DCRTPoly> sk, Ciphertext<DCRTPoly> ct) { Plaintext pt; cc->Decrypt(sk,ct,&pt); pt->SetLength(16); return pt->GetRealPackedValue(); }
double max_err(const std::vector<double>& a, const std::vector<double>& b) { double e=0; for(size_t i=0;i<std::min(a.size(),b.size());i++) e=std::max(e,std::abs(a[i]-b[i])); return e; }
void test_header(const std::string& n) { std::cout<<"\n==========================================================\n  测试: "<<n<<"\n==========================================================\n"; }

bool test_json_add() {
    test_header("两方加法");
        auto cc = setup_ckks(); auto kp = cc->KeyGen(); cc->EvalMultKeyGen(kp.secretKey);
    std::vector<double> d1(16,10.0), d2(16,30.0);
    auto ct1=encrypt(cc,kp.publicKey,d1), ct2=encrypt(cc,kp.publicKey,d2);
    auto ast=ExprParser::parse_json(R"({"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}})");
    HeEvaluator eval(cc,kp.publicKey,{{1,ct1},{2,ct2}});
    auto r=cc->ModReduce(eval.evaluate(ast));
    auto vals=decrypt(cc,kp.secretKey,r);
    double err=max_err(vals,std::vector<double>(16,40.0));
    bool pass=err<1e-4; std::cout<<"  ["<<(pass?"✓ 通过":"✗ 失败")<<"] 两方加法  err="<<std::scientific<<err<<"\n"; return pass;
}
bool test_json_avg() {
    test_header("三方平均");
        auto cc = setup_ckks(); auto kp = cc->KeyGen(); cc->EvalMultKeyGen(kp.secretKey);
    std::vector<double> d1={10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160};
    std::vector<double> d2={5,15,25,35,45,55,65,75,85,95,105,115,125,135,145,155};
    std::vector<double> d3={15,25,35,45,55,65,75,85,95,105,115,125,135,145,155,165};
    auto ct1=encrypt(cc,kp.publicKey,d1), ct2=encrypt(cc,kp.publicKey,d2), ct3=encrypt(cc,kp.publicKey,d3);
    auto ast=ExprParser::parse_json(R"({"op":"div_const","value":3.0,"lhs":{"op":"add","lhs":{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}},"rhs":{"op":"var","party":3}}})");
    HeEvaluator eval(cc,kp.publicKey,{{1,ct1},{2,ct2},{3,ct3}});
    auto r=cc->ModReduce(eval.evaluate(ast));
    auto vals=decrypt(cc,kp.secretKey,r);
    std::vector<double> expected(16); for(size_t i=0;i<16;i++) expected[i]=(d1[i]+d2[i]+d3[i])/3.0;
    double err=max_err(vals,expected);
    bool pass=err<1e-4; std::cout<<"  ["<<(pass?"✓ 通过":"✗ 失败")<<"] 三方平均  err="<<std::scientific<<err<<"\n"; return pass;
}
bool test_json_mixed() {
    test_header("混合 A×B+C");
    auto cc = setup_ckks(4); auto kp = cc->KeyGen(); cc->EvalMultKeyGen(kp.secretKey);
    std::vector<double> d1(16,2.0),d2(16,3.0),d3(16,10.0);
    auto ct1=encrypt(cc,kp.publicKey,d1), ct2=encrypt(cc,kp.publicKey,d2), ct3=encrypt(cc,kp.publicKey,d3);
    auto ast=ExprParser::parse_json(R"({"op":"add","lhs":{"op":"mul","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}},"rhs":{"op":"var","party":3}})");
    HeEvaluator eval(cc,kp.publicKey,{{1,ct1},{2,ct2},{3,ct3}});
    auto r=cc->ModReduce(eval.evaluate(ast));
    double err=max_err(decrypt(cc,kp.secretKey,r),std::vector<double>(16,16.0));
    bool pass=err<1e-4; std::cout<<"  ["<<(pass?"✓ 通过":"✗ 失败")<<"] A×B+C  err="<<std::scientific<<err<<"\n"; return pass;
}
bool test_depth_limit() {
    test_header("深度限制");
    auto v1=AstNode::make_var(1),v2=AstNode::make_var(2);
    auto m1=AstNode::make_binary(OpType::MUL,v1,v2), m2=AstNode::make_binary(OpType::MUL,m1,m1);
    auto m3=AstNode::make_binary(OpType::MUL,m2,m2);
    int d=ExprParser::compute_depth(m3); std::cout<<"  深度="<<d<<" (期望 3)\n";
    bool pass=(d>2); std::cout<<"  ["<<(pass?"✓ 通过":"✗ 失败")<<"] 深度校验 (d="<<d<<">2)\n"; return pass;
}

int main() {
    std::cout<<"==========================================================\n  Task-04 集成测试\n==========================================================\n";
    int passed=0,total=4;
    if(test_json_add())passed++;if(test_json_avg())passed++;if(test_json_mixed())passed++;if(test_depth_limit())passed++;
    std::cout<<"\n==========================================================\n  结果: "<<passed<<" / "<<total<<" 个测试通过\n==========================================================\n";
    return (passed==total)?0:1;
}
