/*
* generated by Xtext
*/
grammar InternalWML;

options {
	superClass=AbstractInternalAntlrParser;
	
}

@lexer::header {
package org.wesnoth.parser.antlr.internal;

// Hack: Use our own Lexer superclass by means of import. 
// Currently there is no other way to specify the superclass for the lexer.
import org.eclipse.xtext.parser.antlr.Lexer;
}

@parser::header {
package org.wesnoth.parser.antlr.internal; 

import java.io.InputStream;
import org.eclipse.xtext.*;
import org.eclipse.xtext.parser.*;
import org.eclipse.xtext.parser.impl.*;
import org.eclipse.xtext.parsetree.*;
import org.eclipse.emf.ecore.util.EcoreUtil;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.xtext.parser.antlr.AbstractInternalAntlrParser;
import org.eclipse.xtext.parser.antlr.XtextTokenStream;
import org.eclipse.xtext.parser.antlr.XtextTokenStream.HiddenTokens;
import org.eclipse.xtext.parser.antlr.AntlrDatatypeRuleToken;
import org.eclipse.xtext.conversion.ValueConverterException;
import org.wesnoth.services.WMLGrammarAccess;

}

@parser::members {

 	private WMLGrammarAccess grammarAccess;
 	
    public InternalWMLParser(TokenStream input, IAstFactory factory, WMLGrammarAccess grammarAccess) {
        this(input);
        this.factory = factory;
        registerRules(grammarAccess.getGrammar());
        this.grammarAccess = grammarAccess;
    }
    
    @Override
    protected InputStream getTokenFile() {
    	ClassLoader classLoader = getClass().getClassLoader();
    	return classLoader.getResourceAsStream("org/wesnoth/parser/antlr/internal/InternalWML.tokens");
    }
    
    @Override
    protected String getFirstRuleName() {
    	return "WMLRoot";	
   	}
   	
   	@Override
   	protected WMLGrammarAccess getGrammarAccess() {
   		return grammarAccess;
   	}
}

@rulecatch { 
    catch (RecognitionException re) { 
        recover(input,re); 
        appendSkippedTokens();
    } 
}




// Entry rule entryRuleWMLRoot
entryRuleWMLRoot returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLRootRule(), currentNode); }
	 iv_ruleWMLRoot=ruleWMLRoot 
	 { $current=$iv_ruleWMLRoot.current; } 
	 EOF 
;

// Rule WMLRoot
ruleWMLRoot returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
((
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLRootAccess().getTagsWMLTagParserRuleCall_0_0(), currentNode); 
	    }
		lv_tags_0_0=ruleWMLTag		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLRootRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"tags",
	        		lv_tags_0_0, 
	        		"WMLTag", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLRootAccess().getMacrosWMLAbstractMacroCallParserRuleCall_1_0(), currentNode); 
	    }
		lv_macros_1_0=ruleWMLAbstractMacroCall		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLRootRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macros",
	        		lv_macros_1_0, 
	        		"WMLAbstractMacroCall", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLRootAccess().getMacrosDefinesWMLMacroDefineParserRuleCall_2_0(), currentNode); 
	    }
		lv_macrosDefines_2_0=ruleWMLMacroDefine		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLRootRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macrosDefines",
	        		lv_macrosDefines_2_0, 
	        		"WMLMacroDefine", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
))*
;





// Entry rule entryRuleWMLTag
entryRuleWMLTag returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLTagRule(), currentNode); }
	 iv_ruleWMLTag=ruleWMLTag 
	 { $current=$iv_ruleWMLTag.current; } 
	 EOF 
;

// Rule WMLTag
ruleWMLTag returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(	'[' 
    {
        createLeafNode(grammarAccess.getWMLTagAccess().getLeftSquareBracketKeyword_0(), null); 
    }
(
(
		lv_plus_1_0=	'+' 
    {
        createLeafNode(grammarAccess.getWMLTagAccess().getPlusPlusSignKeyword_1_0(), "plus"); 
    }
 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLTagRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        
	        try {
	       		set($current, "plus", true, "+", lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }

)
)?(
(
		lv_name_2_0=RULE_ID
		{
			createLeafNode(grammarAccess.getWMLTagAccess().getNameIDTerminalRuleCall_2_0(), "name"); 
		}
		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLTagRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        try {
	       		set(
	       			$current, 
	       			"name",
	        		lv_name_2_0, 
	        		"ID", 
	        		lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }

)
)	']' 
    {
        createLeafNode(grammarAccess.getWMLTagAccess().getRightSquareBracketKeyword_3(), null); 
    }
((
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLTagAccess().getTagsWMLTagParserRuleCall_4_0_0(), currentNode); 
	    }
		lv_tags_4_0=ruleWMLTag		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLTagRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"tags",
	        		lv_tags_4_0, 
	        		"WMLTag", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLTagAccess().getMacrosWMLAbstractMacroCallParserRuleCall_4_1_0(), currentNode); 
	    }
		lv_macros_5_0=ruleWMLAbstractMacroCall		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLTagRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macros",
	        		lv_macros_5_0, 
	        		"WMLAbstractMacroCall", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLTagAccess().getMacrosDefinesWMLMacroDefineParserRuleCall_4_2_0(), currentNode); 
	    }
		lv_macrosDefines_6_0=ruleWMLMacroDefine		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLTagRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macrosDefines",
	        		lv_macrosDefines_6_0, 
	        		"WMLMacroDefine", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLTagAccess().getKeysWMLKeyParserRuleCall_4_3_0(), currentNode); 
	    }
		lv_keys_7_0=ruleWMLKey		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLTagRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"keys",
	        		lv_keys_7_0, 
	        		"WMLKey", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
))*	'[/' 
    {
        createLeafNode(grammarAccess.getWMLTagAccess().getLeftSquareBracketSolidusKeyword_5(), null); 
    }
(
(
		lv_endName_9_0=RULE_ID
		{
			createLeafNode(grammarAccess.getWMLTagAccess().getEndNameIDTerminalRuleCall_6_0(), "endName"); 
		}
		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLTagRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        try {
	       		set(
	       			$current, 
	       			"endName",
	        		lv_endName_9_0, 
	        		"ID", 
	        		lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }

)
)	']' 
    {
        createLeafNode(grammarAccess.getWMLTagAccess().getRightSquareBracketKeyword_7(), null); 
    }
)
;





// Entry rule entryRuleWMLAbstractMacroCall
entryRuleWMLAbstractMacroCall returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLAbstractMacroCallRule(), currentNode); }
	 iv_ruleWMLAbstractMacroCall=ruleWMLAbstractMacroCall 
	 { $current=$iv_ruleWMLAbstractMacroCall.current; } 
	 EOF 
;

// Rule WMLAbstractMacroCall
ruleWMLAbstractMacroCall returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLAbstractMacroCallAccess().getWMLMacroIncludeParserRuleCall_0(), currentNode); 
    }
    this_WMLMacroInclude_0=ruleWMLMacroInclude
    { 
        $current = $this_WMLMacroInclude_0.current; 
        currentNode = currentNode.getParent();
    }

    |
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLAbstractMacroCallAccess().getWMLMacroCallParserRuleCall_1(), currentNode); 
    }
    this_WMLMacroCall_1=ruleWMLMacroCall
    { 
        $current = $this_WMLMacroCall_1.current; 
        currentNode = currentNode.getParent();
    }
)
;





// Entry rule entryRuleWMLMacroInclude
entryRuleWMLMacroInclude returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLMacroIncludeRule(), currentNode); }
	 iv_ruleWMLMacroInclude=ruleWMLMacroInclude 
	 { $current=$iv_ruleWMLMacroInclude.current; } 
	 EOF 
;

// Rule WMLMacroInclude
ruleWMLMacroInclude returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(	'{' 
    {
        createLeafNode(grammarAccess.getWMLMacroIncludeAccess().getLeftCurlyBracketKeyword_0(), null); 
    }
(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroIncludeAccess().getNameWMLPathParserRuleCall_1_0(), currentNode); 
	    }
		lv_name_1_0=ruleWMLPath		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroIncludeRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		set(
	       			$current, 
	       			"name",
	        		lv_name_1_0, 
	        		"WMLPath", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)	'}' 
    {
        createLeafNode(grammarAccess.getWMLMacroIncludeAccess().getRightCurlyBracketKeyword_2(), null); 
    }
)
;





// Entry rule entryRuleWMLMacroCall
entryRuleWMLMacroCall returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLMacroCallRule(), currentNode); }
	 iv_ruleWMLMacroCall=ruleWMLMacroCall 
	 { $current=$iv_ruleWMLMacroCall.current; } 
	 EOF 
;

// Rule WMLMacroCall
ruleWMLMacroCall returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(	'{' 
    {
        createLeafNode(grammarAccess.getWMLMacroCallAccess().getLeftCurlyBracketKeyword_0(), null); 
    }
(
(
		lv_name_1_0=RULE_ID
		{
			createLeafNode(grammarAccess.getWMLMacroCallAccess().getNameIDTerminalRuleCall_1_0(), "name"); 
		}
		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        try {
	       		set(
	       			$current, 
	       			"name",
	        		lv_name_1_0, 
	        		"ID", 
	        		lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }

)
)(
(
(
		lv_args_2_1=RULE_ID
		{
			createLeafNode(grammarAccess.getWMLMacroCallAccess().getArgsIDTerminalRuleCall_2_0_0(), "args"); 
		}
		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"args",
	        		lv_args_2_1, 
	        		"ID", 
	        		lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }

    |		lv_args_2_2=RULE_STRING
		{
			createLeafNode(grammarAccess.getWMLMacroCallAccess().getArgsSTRINGTerminalRuleCall_2_0_1(), "args"); 
		}
		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"args",
	        		lv_args_2_2, 
	        		"STRING", 
	        		lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }

    |		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallAccess().getArgsTSTRINGParserRuleCall_2_0_2(), currentNode); 
	    }
		lv_args_2_3=ruleTSTRING		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"args",
	        		lv_args_2_3, 
	        		"TSTRING", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

    |		lv_args_2_4=RULE_ANY_OTHER
		{
			createLeafNode(grammarAccess.getWMLMacroCallAccess().getArgsANY_OTHERTerminalRuleCall_2_0_3(), "args"); 
		}
		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"args",
	        		lv_args_2_4, 
	        		"ANY_OTHER", 
	        		lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }

)

)
)*((
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallAccess().getParamsWMLMacroCallParameterParserRuleCall_3_0_0(), currentNode); 
	    }
		lv_params_3_0=ruleWMLMacroCallParameter		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"params",
	        		lv_params_3_0, 
	        		"WMLMacroCallParameter", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallAccess().getTagsWMLTagParserRuleCall_3_1_0(), currentNode); 
	    }
		lv_tags_4_0=ruleWMLTag		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"tags",
	        		lv_tags_4_0, 
	        		"WMLTag", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallAccess().getMacrosWMLMacroCallParserRuleCall_3_2_0(), currentNode); 
	    }
		lv_macros_5_0=ruleWMLMacroCall		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macros",
	        		lv_macros_5_0, 
	        		"WMLMacroCall", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallAccess().getMacrosDefinesWMLMacroDefineParserRuleCall_3_3_0(), currentNode); 
	    }
		lv_macrosDefines_6_0=ruleWMLMacroDefine		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macrosDefines",
	        		lv_macrosDefines_6_0, 
	        		"WMLMacroDefine", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallAccess().getKeysWMLKeyParserRuleCall_3_4_0(), currentNode); 
	    }
		lv_keys_7_0=ruleWMLKey		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroCallRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"keys",
	        		lv_keys_7_0, 
	        		"WMLKey", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
))*	'}' 
    {
        createLeafNode(grammarAccess.getWMLMacroCallAccess().getRightCurlyBracketKeyword_4(), null); 
    }
)
;





// Entry rule entryRuleWMLMacroDefine
entryRuleWMLMacroDefine returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLMacroDefineRule(), currentNode); }
	 iv_ruleWMLMacroDefine=ruleWMLMacroDefine 
	 { $current=$iv_ruleWMLMacroDefine.current; } 
	 EOF 
;

// Rule WMLMacroDefine
ruleWMLMacroDefine returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(RULE_DEFINE
    { 
    createLeafNode(grammarAccess.getWMLMacroDefineAccess().getDEFINETerminalRuleCall_0(), null); 
    }
((
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroDefineAccess().getParamsWMLMacroCallParameterParserRuleCall_1_0_0(), currentNode); 
	    }
		lv_params_1_0=ruleWMLMacroCallParameter		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroDefineRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"params",
	        		lv_params_1_0, 
	        		"WMLMacroCallParameter", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroDefineAccess().getTagsWMLTagParserRuleCall_1_1_0(), currentNode); 
	    }
		lv_tags_2_0=ruleWMLTag		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroDefineRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"tags",
	        		lv_tags_2_0, 
	        		"WMLTag", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroDefineAccess().getMacrosWMLMacroCallParserRuleCall_1_2_0(), currentNode); 
	    }
		lv_macros_3_0=ruleWMLMacroCall		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroDefineRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macros",
	        		lv_macros_3_0, 
	        		"WMLMacroCall", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroDefineAccess().getMacrosDefinesWMLMacroDefineParserRuleCall_1_3_0(), currentNode); 
	    }
		lv_macrosDefines_4_0=ruleWMLMacroDefine		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroDefineRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"macrosDefines",
	        		lv_macrosDefines_4_0, 
	        		"WMLMacroDefine", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)
    |(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLMacroDefineAccess().getKeysWMLKeyParserRuleCall_1_4_0(), currentNode); 
	    }
		lv_keys_5_0=ruleWMLKey		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLMacroDefineRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"keys",
	        		lv_keys_5_0, 
	        		"WMLKey", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
))*RULE_ENDDEFINE
    { 
    createLeafNode(grammarAccess.getWMLMacroDefineAccess().getENDDEFINETerminalRuleCall_2(), null); 
    }
)
;







// Entry rule entryRuleWMLKey
entryRuleWMLKey returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLKeyRule(), currentNode); }
	 iv_ruleWMLKey=ruleWMLKey 
	 { $current=$iv_ruleWMLKey.current; } 
	 EOF 
;

// Rule WMLKey
ruleWMLKey returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
((
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLKeyAccess().getNameWMLIDListParserRuleCall_0_0(), currentNode); 
	    }
		lv_name_0_0=ruleWMLIDList		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLKeyRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		set(
	       			$current, 
	       			"name",
	        		lv_name_0_0, 
	        		"WMLIDList", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)	'=' 
    {
        createLeafNode(grammarAccess.getWMLKeyAccess().getEqualsSignKeyword_1(), null); 
    }
(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLKeyAccess().getValueWMLKeyValueParserRuleCall_2_0(), currentNode); 
	    }
		lv_value_2_0=ruleWMLKeyValue		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLKeyRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		set(
	       			$current, 
	       			"value",
	        		lv_value_2_0, 
	        		"WMLKeyValue", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
)(	'+' 
    {
        createLeafNode(grammarAccess.getWMLKeyAccess().getPlusSignKeyword_3_0(), null); 
    }
(
(
		{ 
	        currentNode=createCompositeNode(grammarAccess.getWMLKeyAccess().getExtraArgsWMLKeyExtraArgsParserRuleCall_3_1_0(), currentNode); 
	    }
		lv_extraArgs_4_0=ruleWMLKeyExtraArgs		{
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getWMLKeyRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        try {
	       		add(
	       			$current, 
	       			"extraArgs",
	        		lv_extraArgs_4_0, 
	        		"WMLKeyExtraArgs", 
	        		currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }

)
))*)
;





// Entry rule entryRuleWMLKeyExtraArgs
entryRuleWMLKeyExtraArgs returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLKeyExtraArgsRule(), currentNode); }
	 iv_ruleWMLKeyExtraArgs=ruleWMLKeyExtraArgs 
	 { $current=$iv_ruleWMLKeyExtraArgs.current; } 
	 EOF 
;

// Rule WMLKeyExtraArgs
ruleWMLKeyExtraArgs returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLKeyExtraArgsAccess().getWMLMacroCallParserRuleCall_0(), currentNode); 
    }
    this_WMLMacroCall_0=ruleWMLMacroCall
    { 
        $current = $this_WMLMacroCall_0.current; 
        currentNode = currentNode.getParent();
    }

    |RULE_STRING
    { 
    createLeafNode(grammarAccess.getWMLKeyExtraArgsAccess().getSTRINGTerminalRuleCall_1(), null); 
    }

    |ruleTSTRING)
;





// Entry rule entryRuleWMLMacroCallParameter
entryRuleWMLMacroCallParameter returns [String current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLMacroCallParameterRule(), currentNode); } 
	 iv_ruleWMLMacroCallParameter=ruleWMLMacroCallParameter 
	 { $current=$iv_ruleWMLMacroCallParameter.current.getText(); }  
	 EOF 
;

// Rule WMLMacroCallParameter
ruleWMLMacroCallParameter returns [AntlrDatatypeRuleToken current=new AntlrDatatypeRuleToken()] 
    @init { setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
	    lastConsumedNode = currentNode;
    }:
(
	kw='(' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getWMLMacroCallParameterAccess().getLeftParenthesisKeyword_0(), null); 
    }
(    this_ID_1=RULE_ID    {
		$current.merge(this_ID_1);
    }

    { 
    createLeafNode(grammarAccess.getWMLMacroCallParameterAccess().getIDTerminalRuleCall_1_0(), null); 
    }

    |    this_STRING_2=RULE_STRING    {
		$current.merge(this_STRING_2);
    }

    { 
    createLeafNode(grammarAccess.getWMLMacroCallParameterAccess().getSTRINGTerminalRuleCall_1_1(), null); 
    }

    |
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallParameterAccess().getTSTRINGParserRuleCall_1_2(), currentNode); 
    }
    this_TSTRING_3=ruleTSTRING    {
		$current.merge(this_TSTRING_3);
    }

    { 
        currentNode = currentNode.getParent();
    }

    |
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLMacroCallParameterAccess().getFILEParserRuleCall_1_3(), currentNode); 
    }
    this_FILE_4=ruleFILE    {
		$current.merge(this_FILE_4);
    }

    { 
        currentNode = currentNode.getParent();
    }
)
	kw=')' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getWMLMacroCallParameterAccess().getRightParenthesisKeyword_2(), null); 
    }
)
    ;





// Entry rule entryRuleWMLKeyValue
entryRuleWMLKeyValue returns [EObject current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLKeyValueRule(), currentNode); }
	 iv_ruleWMLKeyValue=ruleWMLKeyValue 
	 { $current=$iv_ruleWMLKeyValue.current; } 
	 EOF 
;

// Rule WMLKeyValue
ruleWMLKeyValue returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(ruleWMLINTList
    |ruleWMLIDList
    |
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLKeyValueAccess().getWMLMacroCallParserRuleCall_2(), currentNode); 
    }
    this_WMLMacroCall_2=ruleWMLMacroCall
    { 
        $current = $this_WMLMacroCall_2.current; 
        currentNode = currentNode.getParent();
    }

    |RULE_STRING
    { 
    createLeafNode(grammarAccess.getWMLKeyValueAccess().getSTRINGTerminalRuleCall_3(), null); 
    }

    |ruleTSTRING
    |ruleWMLPath
    |ruleFILE)
;





// Entry rule entryRuleWMLPath
entryRuleWMLPath returns [String current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLPathRule(), currentNode); } 
	 iv_ruleWMLPath=ruleWMLPath 
	 { $current=$iv_ruleWMLPath.current.getText(); }  
	 EOF 
;

// Rule WMLPath
ruleWMLPath returns [AntlrDatatypeRuleToken current=new AntlrDatatypeRuleToken()] 
    @init { setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
	    lastConsumedNode = currentNode;
    }:
((
	kw='~' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getWMLPathAccess().getTildeKeyword_0(), null); 
    }
)?
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLPathAccess().getPATH_IDParserRuleCall_1(), currentNode); 
    }
    this_PATH_ID_1=rulePATH_ID    {
		$current.merge(this_PATH_ID_1);
    }

    { 
        currentNode = currentNode.getParent();
    }
(
	kw='/' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getWMLPathAccess().getSolidusKeyword_2_0(), null); 
    }

    { 
        currentNode=createCompositeNode(grammarAccess.getWMLPathAccess().getPATH_IDParserRuleCall_2_1(), currentNode); 
    }
    this_PATH_ID_3=rulePATH_ID    {
		$current.merge(this_PATH_ID_3);
    }

    { 
        currentNode = currentNode.getParent();
    }
)+(
    { 
        currentNode=createCompositeNode(grammarAccess.getWMLPathAccess().getFILEParserRuleCall_3(), currentNode); 
    }
    this_FILE_4=ruleFILE    {
		$current.merge(this_FILE_4);
    }

    { 
        currentNode = currentNode.getParent();
    }
)?(
	kw='/' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getWMLPathAccess().getSolidusKeyword_4(), null); 
    }
)?)
    ;





// Entry rule entryRuleWMLIDList
entryRuleWMLIDList returns [String current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLIDListRule(), currentNode); } 
	 iv_ruleWMLIDList=ruleWMLIDList 
	 { $current=$iv_ruleWMLIDList.current.getText(); }  
	 EOF 
;

// Rule WMLIDList
ruleWMLIDList returns [AntlrDatatypeRuleToken current=new AntlrDatatypeRuleToken()] 
    @init { setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
	    lastConsumedNode = currentNode;
    }:
(    this_ID_0=RULE_ID    {
		$current.merge(this_ID_0);
    }

    { 
    createLeafNode(grammarAccess.getWMLIDListAccess().getIDTerminalRuleCall_0(), null); 
    }
(
	kw=',' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getWMLIDListAccess().getCommaKeyword_1_0(), null); 
    }
    this_ID_2=RULE_ID    {
		$current.merge(this_ID_2);
    }

    { 
    createLeafNode(grammarAccess.getWMLIDListAccess().getIDTerminalRuleCall_1_1(), null); 
    }
)*)
    ;





// Entry rule entryRuleWMLINTList
entryRuleWMLINTList returns [String current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getWMLINTListRule(), currentNode); } 
	 iv_ruleWMLINTList=ruleWMLINTList 
	 { $current=$iv_ruleWMLINTList.current.getText(); }  
	 EOF 
;

// Rule WMLINTList
ruleWMLINTList returns [AntlrDatatypeRuleToken current=new AntlrDatatypeRuleToken()] 
    @init { setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
	    lastConsumedNode = currentNode;
    }:
(    this_INT_0=RULE_INT    {
		$current.merge(this_INT_0);
    }

    { 
    createLeafNode(grammarAccess.getWMLINTListAccess().getINTTerminalRuleCall_0(), null); 
    }
(
	kw=',' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getWMLINTListAccess().getCommaKeyword_1_0(), null); 
    }
    this_INT_2=RULE_INT    {
		$current.merge(this_INT_2);
    }

    { 
    createLeafNode(grammarAccess.getWMLINTListAccess().getINTTerminalRuleCall_1_1(), null); 
    }
)*)
    ;





// Entry rule entryRuleTSTRING
entryRuleTSTRING returns [String current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getTSTRINGRule(), currentNode); } 
	 iv_ruleTSTRING=ruleTSTRING 
	 { $current=$iv_ruleTSTRING.current.getText(); }  
	 EOF 
;

// Rule TSTRING
ruleTSTRING returns [AntlrDatatypeRuleToken current=new AntlrDatatypeRuleToken()] 
    @init { setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
	    lastConsumedNode = currentNode;
    }:
(
	kw='_' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getTSTRINGAccess().get_Keyword_0(), null); 
    }
    this_STRING_1=RULE_STRING    {
		$current.merge(this_STRING_1);
    }

    { 
    createLeafNode(grammarAccess.getTSTRINGAccess().getSTRINGTerminalRuleCall_1(), null); 
    }
)
    ;





// Entry rule entryRuleFILE
entryRuleFILE returns [String current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getFILERule(), currentNode); } 
	 iv_ruleFILE=ruleFILE 
	 { $current=$iv_ruleFILE.current.getText(); }  
	 EOF 
;

// Rule FILE
ruleFILE returns [AntlrDatatypeRuleToken current=new AntlrDatatypeRuleToken()] 
    @init { setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
	    lastConsumedNode = currentNode;
    }:
((
    { 
        currentNode=createCompositeNode(grammarAccess.getFILEAccess().getPATH_IDParserRuleCall_0(), currentNode); 
    }
    this_PATH_ID_0=rulePATH_ID    {
		$current.merge(this_PATH_ID_0);
    }

    { 
        currentNode = currentNode.getParent();
    }
)+
	kw='.' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getFILEAccess().getFullStopKeyword_1(), null); 
    }
    this_ID_2=RULE_ID    {
		$current.merge(this_ID_2);
    }

    { 
    createLeafNode(grammarAccess.getFILEAccess().getIDTerminalRuleCall_2(), null); 
    }
)
    ;





// Entry rule entryRulePATH_ID
entryRulePATH_ID returns [String current=null] 
	:
	{ currentNode = createCompositeNode(grammarAccess.getPATH_IDRule(), currentNode); } 
	 iv_rulePATH_ID=rulePATH_ID 
	 { $current=$iv_rulePATH_ID.current.getText(); }  
	 EOF 
;

// Rule PATH_ID
rulePATH_ID returns [AntlrDatatypeRuleToken current=new AntlrDatatypeRuleToken()] 
    @init { setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
	    lastConsumedNode = currentNode;
    }:
(    this_ID_0=RULE_ID    {
		$current.merge(this_ID_0);
    }

    { 
    createLeafNode(grammarAccess.getPATH_IDAccess().getIDTerminalRuleCall_0(), null); 
    }

    |
	kw='-' 
    {
        $current.merge(kw);
        createLeafNode(grammarAccess.getPATH_IDAccess().getHyphenMinusKeyword_1(), null); 
    }
)
    ;





RULE_ID : ('a'..'z'|'A'..'Z'|'_'|'0'..'9')+;

RULE_INT : ('0'..'9')* ('.' ('0'..'9')+)?;

RULE_STRING : '"' ('\\' ('b'|'t'|'n'|'f'|'r'|'"'|'\''|'\\')|~(('\\'|'"')))* '"';

RULE_TEXTDOMAIN : '#textdomain' ~(('\n'|'\r'))* ('\r'? '\n')?;

RULE_DEFINE : '#define' ~(('\n'|'\r'))* ('\r'? '\n')?;

RULE_ENDDEFINE : '#enddef' ~(('\n'|'\r'))* ('\r'? '\n')?;

RULE_SL_COMMENT : '#' ~(('\n'|'\r'))* ('\r'? '\n')?;

RULE_WS : (' '|'\t'|'\r'|'\n')+;

RULE_ANY_OTHER : .;


